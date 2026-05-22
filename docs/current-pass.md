# ResonantNode Post-Cleanup Bug / Drift List

Status: complete

This pass is closed; future cleanup should start in a new pass.

Intent: prioritize remaining bugs and roadmap/spec drift after the recent cleanup pass. This list is ordered by practical impact: first restore testability, then fix Analyzer truth, then remove profile/runtime drift, then simplify names/logs/dead code.

## Scope baseline

Accepted architecture / cleanup decisions still apply:

```text
DetectionRuntime owns detection truth.
PatternRules own patternMatched / supportMatched / valid.
Analyzer measures ExpectedEvent + PatternResult + timing; it does not redo detection.
Behavior consumes PatternResult + FieldState and owns behaviorEligible.
FreqAmpProfile = FrequencyMatch primary + AMP support inspection + SinglePulseOnly.
AmpSignalEmitter is not active in FreqAmp.
ChirpProfile / AmpStateProfile may exist as proof/future concepts, but must not falsely affect stable FreqAmp runtime.
Runtime timestamps use one local node timebase: wall-clock sample time in uint32_t ms.
```

---

# P0 — Testability / broken command path

## 1. SEQ command parsing likely broken

### Bug

`SEQ` commands appear to return help immediately instead of processing subcommands.

### Details

In `AnalyzerCommands.cpp` / `handleUsbLine()` style logic, the parser enters the `SEQ` branch, tokenizes the command, then checks the first token. For a command like:

```text
SEQ start tries=100
```

the first token is always:

```text
SEQ
```

If the code checks `token == "SEQ"` as a help condition and returns, the parser never reaches `start`, `obs`, `stop`, etc.

### Suggested fix

Consume the `SEQ` token first, then parse the subcommand:

```cpp
char* token = strtok_r(buffer, " ", &savePtr); // "SEQ"
char* subcommand = strtok_r(nullptr, " ", &savePtr); // "START" / "OBS" / "STOP" / "HELP"

if (subcommand == nullptr || equalsIgnoreCase(subcommand, "HELP")) {
    printSequenceHelp();
    return;
}

if (equalsIgnoreCase(subcommand, "START")) { ... }
if (equalsIgnoreCase(subcommand, "OBS")) { ... }
if (equalsIgnoreCase(subcommand, "STOP")) { ... }
```

### Test / check

Verify these commands do not print help unless requested:

```text
SEQ help
SEQ start tries=5
SEQ obs tries=5
SEQ stop
```

Acceptance:

```text
SEQ start starts a sequence.
SEQ obs starts observation mode.
SEQ stop stops the active sequence.
```

Status: DONE

---

# P1 — Analyzer truth / classification correctness

## 2. Analyzer still counts invalid/rejected PatternResults as trial hits

### Bug

`handleSequenceCandidate()` still appears to set `currentTrialHit = true` for an in-window `PatternResult` without requiring:

```cpp
patternResult.valid == true
```

### Details

This violates the accepted hit gate:

```text
Only PatternResult.valid counts as Analyzer hit and Behavior input.
```

Current risk:

```text
PatternResult arrives in expected window
→ Analyzer stores acceptedTransient/acceptedPattern fields
→ currentTrialHit = true
→ trial becomes expected/late
```

even if:

```text
candidateAccepted=false
patternMatched=false
supportMatched=false
valid=false
reason=missing_amp_support / amp_support_too_low / rejected
```

### Suggested fix

In `handleSequenceCandidate()` split observation from hit classification:

```cpp
const bool inWindow = ...;
const bool validHit = patternResult.valid;

recordCandidateObservation(patternResult, timing, inWindow);

if (!validHit) {
    recordRejectedPatternObservation(patternResult, timing, inWindow);
    return;
}

if (inWindow) {
    capturePrimaryValidPattern(patternResult, timing);
}
```

Do not set the primary hit fields for invalid/rejected PatternResults.

Recommended state names:

```cpp
bool primaryValidPatternCaptured;
PatternResult primaryValidPattern;
uint32_t primaryValidPatternDtMs;
uint32_t rejectedInWindowCount;
PatternResult firstRejectedInWindow;
```

### Test / check

Run a case where frequency matches but AMP support is too low / unknown.

Expected:

```text
SEQ_TRIAL result=miss or rejected
rejected_count > 0
supportMatched=false
valid=false
```

Not acceptable:

```text
SEQ_TRIAL result=expected with valid=false
```

Status: DONE

---

## 3. Unexpected candidates still override a valid expected hit

### Bug

Final trial classification still gives `unexpected` priority over `expected/late` if any unexpected candidate occurred.

### Details

Current dangerous shape:

```cpp
if (invalidAudioTrial) result = Invalid;
else if (unexpectedTrial) result = Unexpected;
else if (hitTrial) result = Expected/Late;
```

This violates the accepted decision:

```text
unexpected / duplicate / rejected are flags/counters, not primary result overrides.
```

A valid in-window hit should remain the primary result even if there are extra out-of-window candidates.

### Suggested fix

Classify primary result first:

```text
primary result = expected / late / early / miss / rejected / invalid_audio
flags = unexpected_count, duplicate_count, rejected_count
```

Suggested order:

```cpp
if (invalidAudioTrial) {
    result = AnalyzerResult::InvalidAudio;
} else if (primaryValidPatternCaptured) {
    result = classifyTiming(primaryValidPatternDtMs);
} else if (rejectedInWindowCount > 0) {
    result = AnalyzerResult::Rejected;
} else {
    result = AnalyzerResult::Miss;
}

report.unexpectedCount = currentTrialUnexpected;
report.duplicateCount = currentTrialDuplicate;
report.rejectedCount = currentTrialRejected;
```

### Test / check

Create/observe a trial with:

```text
one valid in-window result
one extra out-of-window candidate
```

Expected:

```text
result=expected
unexpected=1
```

Not:

```text
result=unexpected
```

Status: DONE

---

## 4. Analyzer still uses old mutable `currentTrialHit` truth instead of stored primary PatternResult

### Bug

Trial classification still appears to derive from mutable booleans and old `acceptedTransient*` fields instead of a stored primary valid `PatternResult`.

### Details

Current shape:

```text
PatternResult arrives
→ handleSequenceCandidate mutates currentTrialHit and acceptedTransient fields
→ finalizeSequenceTrial reads currentTrialHit
```

This preserves the old direct-transient detector mental model.

### Suggested fix

Store trial-level observations explicitly:

```cpp
struct SequencePrimaryObservation {
    bool hasValidPattern = false;
    PatternResult pattern = {};
    uint32_t dtMs = 0;
};

struct SequenceRejectedObservation {
    uint32_t inWindowCount = 0;
    uint32_t outOfWindowCount = 0;
    PatternResult firstInWindow = {};
};
```

Then classify from:

```text
ExpectedEvent + SequencePrimaryObservation + rejected/unexpected/duplicate counters
```

### Test / check

Review `finalizeSequenceTrial()` and ensure it does not infer trial truth from `acceptedTransient*` or `currentTrialHit` without checking `PatternResult.valid`.

Status: DONE

---

# P2 — Profile/runtime drift

## 5. `ChirpProfile` is still exposed as normal although not truly implemented

### Bug / drift

Help and commands still expose:

```text
profile=freqamp|chirp
```

but `ChirpProfile` is not yet a real stable profile composition with proper chirp-specific pattern rules and assembler behavior.

### Details

The updated roadmap treats `ChirpProfile` as a proof/future profile. It should not silently behave as if it were stable.

Current risk:

```text
User selects chirp
logs claim chirp composition
runtime still mostly uses generic/current PatternRules behavior
results are misleading
```

### Suggested fix

Near-term options:

Option A — hide/park:

```text
Expose only profile=freqamp in help and normal commands.
Keep makeChirpProfile() in code as parked/proof if needed.
```

Option B — mark explicit experimental:

```text
profile=chirp_experimental
```

and print:

```text
WARNING: chirp profile is proof/future; not stable runtime profile.
```

Recommended for cleanup: Option A.

### Test / check

`RB help` and `SEQ help` should not advertise `chirp` as stable.

Status: DONE

---

## 6. `ProfilePatternRulesKind` appears decorative / not applied

### Bug / drift

`DetectionProfile` still contains something like:

```cpp
ProfilePatternRulesKind patternRules;
```

but runtime appears to apply only `PatternRulesConfig`, not the pattern-rules kind.

### Details

This means logs/profile summaries may say:

```text
patternRules=ChirpSequence
```

while runtime still uses the same concrete `PatternRules` implementation.

This violates the human-engineer rule:

```text
profile composition must be real or parked
```

### Suggested fix

Either make the field real:

```cpp
_detection.setPatternRulesKind(profile.patternRules);
```

with actual runtime behavior differences,

or remove it from active profile output and profile structs until real.

Recommended now:

```text
Keep PatternRulesConfig as the real applied config.
Do not print/store decorative PatternRulesKind as active truth unless it changes runtime behavior.
```

### Test / check

Profile startup logs should only show composition fields that are actually applied.

Status: DONE

---

## 7. PatternAssembler mode is implicit, not explicit

### Bug / drift

FreqAmp currently behaves as SinglePulseOnly, which is correct, but there is no explicit applied `PatternAssemblerMode` visible in runtime config.

### Details

The accepted profile-composition direction says profile should select assembler behavior. The current code is acceptable if only one assembler mode exists, but profile logs must not imply configurable assembler behavior unless actually applied.

### Suggested fix

Either:

```cpp
enum class PatternAssemblerMode { SinglePulseOnly, PulseSequence };
```

and apply it to `PatternAssembler`,

or keep assembler non-configurable for now and do not log/advertise assembler kind as active profile composition.

Recommended now:

```text
If ChirpProfile is parked, keep assembler simple and non-configurable.
If assembler kind is logged, make it real.
```

### Test / check

FreqAmp should not emit PulseSequence candidates.

Status: DONE

---

# P3 — Node/RB runtime and diagnostics

## 8. Node AMP diagnostic probe exists but may not be observed

### Bug / drift

`Node` owns an `AmpDiagnosticProbe`, configures/prints it, but it appears not to call:

```cpp
_ampDiagnosticProbe.observe(frame);
```

### Details

This means Node/RB AMP diagnostic output may show thresholds/config but no real observation data.

Earlier decision:

```text
AMP diagnostic may stay, but only as explicit diagnostic probe.
```

A half-wired diagnostic probe creates confusion.

### Suggested fix

Choose one:

Option A — implement diagnostic observation gated by debug/AMPDIAG:

```cpp
if (_ampDiagEnabled) {
    _ampDiagnosticProbe.observe(frame);
}
```

Option B — remove Node-owned AMP diagnostic probe and keep AMPDIAG only in Analyzer.

Recommended:

```text
Keep it only if an explicit RB/AMPDIAG command needs it.
Otherwise remove from Node and keep Analyzer diagnostics.
```

### Test / check

If kept:

```text
AMPDIAG on
RB/AMPDIAG status shows nonzero observation counters when sound occurs.
AMPDIAG off
no diagnostic CPU work / no probe observation
```

Status: DONE

---

## 9. Own-chirp suppression uses loop `now` instead of frame time

### Bug / drift

Node now suppresses detection during own emit, which is good, but the check appears based on loop `now`, not per-frame `frame.sampleTimeMs`.

### Details

With backlog-aware audio timing, a frame may represent a sample older than current loop time. Suppression should gate the audio frame by the frame’s event time.

### Suggested fix

Use:

```cpp
const bool ownEmitSuppressed =
    frame.sampleTimeMs < _behavior.ownEmitDetectionSuppressUntilMs();
```

instead of a loop-level `now` check where practical.

### Test / check

During own chirp, detection/FieldState should not ingest frames whose sample time falls inside the suppression window.

Status: DONE

---

## 10. RB counters still mix accepted patterns and emitted chirps

### Bug

`_rbActionCount` still appears to increment both when a pattern candidate is accepted and when a chirp actually starts.

### Details

This makes runtime stats misleading:

```text
accepted pattern candidate != emitted chirp action
```

### Suggested fix

Split counters:

```cpp
_rbCandidateCount;
_rbPatternAcceptedCount;
_rbValidPatternCount;
_rbChirpStartedCount;
```

Increment:

```text
candidateCount: every PatternResult observed
patternAcceptedCount: candidateAccepted true
validPatternCount: result.valid true
chirpStartedCount: output actually started
```

### Test / check

With behavior disabled/listen-only:

```text
validPatternCount may increase
chirpStartedCount must stay 0
```

Status: DONE

---

## 11. RB log mode still exposes fake `off|minimal|full`

### Bug

Help says:

```text
RB log off|minimal|full
```

but enum still appears to have only:

```cpp
Minimal
Full
```

and `rbLogModeName()` maps non-full to `off`.

### Suggested fix

Either add real `Off`:

```cpp
enum class RbLogMode { Off, Minimal, Full };
```

or remove `off` from help and use:

```text
RB log minimal|full
```

Recommended:

```text
Add real Off, keep Minimal, Full.
```

### Test / check

```text
RB log off     -> no RB runtime logs
RB log minimal -> compact logs
RB log full    -> detailed logs
RB status      -> reports correct mode name
```

Status: DONE

---

## 12. `RB DETECT` command/status remains as old vocabulary

### Bug / drift

`RB DETECT` remains in help/commands, even though active architecture is profile/runtime-based.

### Details

This is not necessarily a functional bug, but it preserves the old DetectionMode vocabulary.

### Suggested fix

Rename/remove:

```text
RB DETECT status -> RB DETECTION status
or RB PROFILE status
or RB STATUS
```

Do not expose `RB DETECT` as if old detection mode selection exists.

### Test / check

Help text should not imply old DetectionMode or AMP-detector runtime.

Status: DONE

---

## 13. Baseline state updates before audio processing

### Bug / drift

Node updates RB baseline/quiet state before reading and processing current audio blocks.

### Details

This means baseline logic uses previous-loop signal values.

Severity: low/medium.

### Suggested fix

Move baseline update after audio processing if it does not disturb startup behavior:

```text
read/process audio
update baseline/quiet state from fresh AudioSignal
run behavior/output
```

If changing order is risky, leave for later but document.

### Test / check

Startup/rebase still behaves correctly; no false ready/quiet state.

Status: DONE

---

# P4 — Analyzer cleanup / dead paths / reporting

## 14. Dead `handleSequenceTransient()` still exists

### Bug / drift

`handleSequenceTransient()` still exists but appears not to be called.

### Details

It is old direct-AMP-trial-truth logic and still mutates old fields such as:

```text
currentTrialHit
acceptedTransientMs
```

Even unreachable, it preserves the old mental model and creates future misuse risk.

### Suggested fix

Delete it, or move diagnostic-only parts into AMPDIAG / SEQ_EXPLAIN with names that cannot affect trial classification.

### Test / check

Search:

```text
handleSequenceTransient
currentTrialHit
acceptedTransient
```

Ensure no dead direct-detector trial truth remains.

Status: DONE

---

## 15. Analyzer still uses `acceptedTransient*` names for PatternResult-based truth

### Bug / drift

Analyzer fields still use old transient naming while storing runtime PatternResult-derived observations.

Examples:

```text
acceptedTransientMs
acceptedTransientDurationMs
acceptedTransientStrength
transientAccepted
```

### Suggested fix

Rename runtime truth fields:

```text
acceptedPatternMs
acceptedPatternDurationMs
acceptedPatternStrength
patternAccepted
primaryPatternResult
```

Keep transient-specific names only under AMP diagnostic observation structs.

### Test / check

`acceptedTransient*` should only exist in AMP diagnostic code, not Analyzer primary classification.

Status: DONE

---

## 16. H3 helper names remain in Analyzer reporting

### Bug / drift

Functions named like:

```text
h3SequenceCandidateClass...
```

remain. These are stale report-format names, not architecture terms.

### Suggested fix

Rename to neutral terms:

```text
sequenceCandidateClass...
sequencePatternClass...
printFrequencyEvidenceFields...
```

or delete if replaced by gate-chain reporting.

### Test / check

No `h3*` helper names in active code unless there is a documented reason.

Status: DONE

---

## 17. Duplicated SEQ_CAND reporting blocks

### Bug / drift

`handleSequenceCandidate()` still prints multiple similar blocks:

```text
role=detector
role=pattern
role=result
```

with overlapping fields.

### Suggested fix

Collapse to a smaller stage-chain report:

```text
SEQ_CAND stage=signal ... candidateAccepted=...
SEQ_CAND stage=pattern ... patternMatched=... supportMatched=... valid=... reason=...
```

or one compact line:

```text
SEQ_CAND dt=... candidateAccepted=... patternMatched=... supportMatched=... valid=... reason=...
```

Keep `SEQ_EXPLAIN` detailed, but avoid triple duplicate output.

### Test / check

SEQ output remains readable and includes gate chain.

Status: DONE

---

# P5 — Gate/profile naming drift

## 18. `annotateAmpSupportAndLocality()` still mentions Locality

### Bug / drift

`LocalityClass` is gone, but method names still mention locality.

### Suggested fix

Rename:

```cpp
annotateAmpSupportAndLocality(...)
```

to:

```cpp
annotateAmpSupport(...)
```

Update comments/logs accordingly.

### Test / check

No `Locality` in active architecture code unless explicitly historical.

Status: DONE

---

## 19. `FromAcceptedTransient` reason remains in frequency-first PatternRules

### Bug / drift

Pattern reason names still mention transient even for frequency-first FreqAmp results.

### Suggested fix

Rename to a source-neutral or frequency-specific reason:

```text
FromAcceptedSignal
FromFrequencyMatch
PatternMatched
ValidPattern
```

Pick one based on enum semantics.

### Test / check

FreqAmp valid result should not report `FromAcceptedTransient`.

Status: DONE

---

## 20. Behavior helper still says `transientDetected`

### Bug / drift

Behavior helper names still use old transient wording:

```text
behaviorGateName(..., transientDetected, ...)
```

### Suggested fix

Rename to pattern terms:

```text
patternDetected
validPatternHeard
patternAvailable
```

Behavior should not expose raw transient vocabulary.

### Test / check

Behavior code consumes and names `PatternResult`, not transients.

Status: DONE

---

# P6 — Rollover / timing hardening

## 21. Duplicate-risk elapsed uses non-rollover-safe comparison

### Bug

SignalInspector duplicate-risk code appears to calculate elapsed time like:

```cpp
acceptedMs > lastAcceptedMs ? acceptedMs - lastAcceptedMs : 0
```

This fails over `uint32_t` rollover.

### Suggested fix

Use unsigned subtraction:

```cpp
const uint32_t elapsedMs = static_cast<uint32_t>(acceptedMs - lastAcceptedMs);
```

Then compare elapsed durations.

### Test / check

Unit/small test with artificial wrap values:

```text
lastAcceptedMs = 0xFFFFFFF0
acceptedMs     = 0x00000020
elapsed         = 0x30
```

Status: DONE

---

## 22. General deadline comparisons should use rollover-safe helpers

### Bug / risk

Some code still uses direct comparisons:

```cpp
now >= deadline
now < suppressUntil
now - started > duration
```

Some are acceptable for short runs, but not consistently rollover-safe.

### Suggested fix

Introduce small helpers:

```cpp
inline bool elapsedSince(uint32_t now, uint32_t then, uint32_t durationMs) {
    return static_cast<uint32_t>(now - then) >= durationMs;
}

inline bool beforeDeadline(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) < 0;
}
```

Use for behavior gates, refractory, suppression, sequence windows where practical.

For Analyzer windows, prefer relative dt:

```cpp
uint32_t dt = eventMs - expectedTriggerMs;
bool inWindow = dt >= windowStartOffsetMs && dt <= windowEndOffsetMs;
```

### Test / check

No absolute window check like:

```cpp
eventMs >= windowStartMs && eventMs <= windowEndMs
```

for runtime event classification unless explicitly safe.

Status: DONE

---

# P7 — Output / legacy comments

## 23. `ChirpOutput` still has legacy/dead wording or unreachable phases

### Bug / drift

Earlier review found `ChirpOutput` comments like:

```text
Legacy placeholder
```

and possibly unreachable phase states.

### Suggested fix

Review `ChirpOutput` state machine:

- If phases 2/3 are unreachable, delete them.
- If still used, rename/comment them accurately.
- Remove `Legacy placeholder` wording from active code.

### Test / check

Response chirp / idle chirp still plays as expected.

Status: DONE

---

# P8 — Packaging / source hygiene

## 24. Review ZIP still includes package garbage

### Bug / hygiene

Uploaded ZIP still contains:

```text
.git/
src.zip
src (2).zip
```

### Suggested fix

Add/verify `.gitignore` and export clean source packages without nested source archives or `.git`.

Recommended `.gitignore` additions:

```gitignore
.pio/
*.zip
.vscode/.browse.c_cpp.db*
```

### Test / check

Review ZIP should include project source and config only, not build artifacts or nested copies.

Status: DONE

---

# Recommended implementation order

```text
1. Fix SEQ command parser.
2. Fix Analyzer hit gate: only PatternResult.valid counts.
3. Fix unexpected/rejected/duplicate as flags/counters, not primary overrides.
4. Replace currentTrialHit/acceptedTransient truth with primaryValidPattern observation.
5. Park/hide ChirpProfile from stable help/commands or mark experimental.
6. Remove/decorate only real-applied profile composition fields.
7. Decide Node AMP diagnostic: wire as gated AMPDIAG or remove from Node.
8. Use frame.sampleTimeMs for own-chirp detection suppression.
9. Split RB counters.
10. Fix RB log off/minimal/full.
11. Delete dead handleSequenceTransient path.
12. Rename acceptedTransient/H3/Locality/FromAcceptedTransient/transientDetected leftovers.
13. Add rollover-safe helpers and patch high-value timing checks.
14. Clean ChirpOutput legacy/dead phases.
15. Clean package ZIP/source hygiene.
```

# Done criteria

This cleanup stage is done when:

```text
SEQ commands work again.
Analyzer never reports expected/late from valid=false PatternResult.
Unexpected/rejected/duplicate are counters/flags, not primary overrides.
FreqAmp is the only stable advertised runtime profile.
Profile logs only show config that is actually applied.
AMP diagnostics are either explicitly gated or removed from Node.
RB counters distinguish detected patterns from emitted chirps.
Old transient/H3/locality wording is gone from active truth paths.
Timing checks use frame/sample time and rollover-safe elapsed comparisons where relevant.
```
