# ResonantNode Cleanup Pass — D31–D39

## Purpose

This pass cleans the remaining old AMP-transient / Analyzer / RB-status residue after the TonalPulse / Occurrence / PatternResult cleanup.

The goal is not to change TonalPulse detection behavior. The goal is to make the remaining diagnostic paths explicit, remove stale compatibility/status leftovers, and align active docs with the current profile status.

## Scope

Apply the accepted decisions D31–D39:

```text
D31 Keep AmpDiagnosticProbe in Analyzer only as transitional AMPDIAG / SEQ_EXPLAIN support.
D32 Separate AMP diagnostic reasons from primary SEQ result.
D33 Analyzer must not depend directly on AmpTransientDetector::TransientRejectReason.
D34 Remove currentTrialHit; use primaryValidPatternCaptured as trial truth.
D35 Remove RB DETECT removed-command branch entirely.
D36 Remove detectOnly=0 from RB status unless a real DetectionOnly mode exists.
D37 Delete unused live-frequency constants/comments if unused.
D38 Active docs profile status: TonalPulse stable, ChirpExperimental selectable experimental, AmpState roadmap-only.
D39 Add AmpDiagnosticProbe sunset condition.
```

## Non-goals

Do not implement:

```text
- ParamStore / ParamRegistry
- WiFi / remote param update
- firmware OTA
- VEKTOR integration
- new detection profiles
- full Chirp implementation
- AmpStateProfile implementation
- BehaviorRuntime / OutputDispatcher framework
- generic rule engine
```

Do not retune detection thresholds.

Do not change PatternRules behavior except where names/diagnostic ownership require it.

---

# Pass order

## Pass 1 — Analyzer AMP diagnostic boundary

### Problem

Analyzer still uses the local AMP transient diagnostic path for miss/reject explanation. That is allowed for now, but it must be clearly diagnostic and must not look like trial truth.

Current risk areas:

```text
- Analyzer uses AmpTransientDetector::TransientRejectReason directly.
- Helper names such as noteSequenceTransientReject* imply trial/sequence truth.
- Primary result names may encode AMP diagnostic reasons, e.g. miss_no_onset / miss_weak / miss_too_long.
```

### Required changes

Keep:

```text
AmpDiagnosticProbe in Analyzer
AMPDIAG / SEQ_EXPLAIN diagnostic use
```

Do not keep:

```text
Analyzer direct dependency on AmpTransientDetector::TransientRejectReason
AMP transient diagnostic as primary trial result
```

Introduce or expose diagnostic-owned types, for example:

```cpp
namespace detection {

enum class AmpDiagnosticRejectReason {
    None,
    TooShort,
    TooLong,
    Weak,
    NoOnset,
    // map only what is needed by Analyzer reports
};

struct AmpDiagnosticObservation {
    bool transientObserved = false;
    uint32_t onsetMs = 0;
    uint32_t acceptedMs = 0;
    uint32_t durationMs = 0;
    float strength = 0.0f;
    AmpDiagnosticRejectReason rejectReason = AmpDiagnosticRejectReason::None;
};

}
```

The internal `AmpDiagnosticProbe` may still wrap `AmpTransientDetector`, but Analyzer should depend only on `AmpDiagnosticProbe` / `AmpDiagnosticObservation` / `AmpDiagnosticRejectReason`.

### Rename helpers

Rename old Analyzer helper names:

```text
noteSequenceTransientReject* -> noteAmpDiagnosticReject*
strongestRejectReason        -> strongestAmpDiagRejectReason
lastTransientRejectReason    -> lastAmpDiagRejectReason
```

Keep the behavior the same unless the old names were misleading the result classification.

### Acceptance checks

```text
- Analyzer compiles without referencing AmpTransientDetector::TransientRejectReason directly.
- AmpDiagnosticProbe remains Analyzer-only.
- No RB/Node runtime dependency on AmpDiagnosticProbe.
- SEQ_EXPLAIN can still show AMP diagnostic details.
- SEQ_TRIAL primary result is not decided from AMP diagnostic observation.
```

---

## Pass 2 — Separate primary SEQ result from ampdiag reason

### Problem

Some Analyzer classification names still encode old AMP transient diagnostic reasons as if they were primary trial outcomes.

Bad shape:

```text
result=miss_no_onset
result=miss_weak
result=miss_too_long
```

Better shape:

```text
result=miss
pattern_reason=<PatternResult / TonalPulse reason>
ampdiag_reason=no_onset | weak | too_long | none
```

### Required changes

Primary SEQ result vocabulary should stay generic:

```text
expected
early
late
miss
rejected
unexpected
invalid_audio
ambiguous / too_dense only if already real
```

AMP diagnostic details should move to separate fields:

```text
ampdiag_seen=0|1
ampdiag_reason=no_onset|weak|too_short|too_long|none
ampdiag_duration_ms=...
ampdiag_strength=...
```

Pattern/TonalPulse-native reason should also be visible:

```text
pattern_reason=missing_amp_support|amp_support_too_low|frequency_score_low|...
amp_support=unknown|none|weak|medium|strong
supportMatched=0|1
patternMatched=0|1
valid=0|1
```

### Important rule

Analyzer may classify trial outcome from:

```text
ExpectedEvent + primaryValidPatternCaptured + PatternResult.valid + timing window
```

Analyzer must not classify primary trial outcome from:

```text
AmpDiagnosticProbe observation
AmpDiagnosticRejectReason
old AMP transient accept/reject state
```

### Acceptance checks

```text
- No primary SEQ result value is named miss_no_onset / miss_weak / miss_too_long.
- Those details appear only as ampdiag_reason or AMPDIAG detail fields.
- A valid PatternResult can produce expected/late even if ampdiag has a diagnostic reject.
- An AMP diagnostic transient alone cannot produce expected/late.
```

---

## Pass 3 — Remove `currentTrialHit`

### Problem

`primaryValidPatternCaptured` is now the real trial truth, but `currentTrialHit` remains as a leftover/mirror field.

This creates two hit concepts.

### Required changes

Remove:

```text
currentTrialHit
```

Use only:

```text
primaryValidPatternCaptured
primaryValidPattern
primaryValidPatternDtMs
```

Rejected, duplicate, unexpected, and ampdiag observations remain side counters/details.

### Acceptance checks

```text
- No currentTrialHit member or local remains.
- finalizeSequenceTrial() uses primaryValidPatternCaptured for hit truth.
- rejected / unexpected / duplicate are flags/counters, not hit truth.
```

---

## Pass 4 — Remove RB DETECT branch completely

### Problem

`RB DETECT` no longer works as an alias, but code may still contain a special branch printing:

```text
ERR RB DETECT removed; use RB STATUS
```

Accepted decision: no compatibility sediment.

### Required changes

Remove the special `RB DETECT` parser branch entirely.

Let generic unknown-command handling deal with it.

Keep only:

```text
RB STATUS
```

### Acceptance checks

```text
- Searching src/ for "RB DETECT" returns no active command/parser/help references.
- RB STATUS still works.
- Docs/manual do not mention RB DETECT except historical archive if untouched.
```

---

## Pass 5 — Remove `detectOnly=0` from RB status

### Problem

RB status still prints a fake/static value:

```text
detectOnly=0
```

If there is no real DetectionOnly mode, this is stale architecture residue.

### Required changes

Remove `detectOnly` from RB status/log output.

Only print real active runtime states.

If a real detection-only mode exists, print the actual value from the real mode variable. Do not print a hardcoded constant.

### Acceptance checks

```text
- No hardcoded detectOnly=0 output remains.
- RB STATUS still prints useful profile/state/counter information.
```

---

## Pass 6 — Delete unused live-frequency constants/comments

### Problem

Old live-frequency constants may remain in `node.cpp`, such as:

```text
kLiveFrequencyReleaseDebounceMs
kLiveFrequencyCooldownAfterOnsetMs
kLiveFrequencyMinTransientDurationMs
```

If unused, they preserve old transient/frequency tuning assumptions.

### Required changes

Check all `kLiveFrequency*` constants.

If unused:

```text
- delete constants
- delete comments that describe old live-frequency/transient behavior
```

If used:

```text
- keep them
- rename/comment them according to current TonalPulse/FrequencyOccurrence terminology
```

### Acceptance checks

```text
- No unused kLiveFrequency* constants remain.
- No misleading old transient-frequency comments remain in active runtime code.
```

---

## Pass 7 — Minor source cleanup

### Problem

Small old-code residue remains.

Examples:

```text
empty anonymous namespace in AmpOccurrenceSource.cpp
old "Legacy placeholder" comments
comments that describe old architecture as current
```

### Required changes

Remove trivial empty namespaces and stale comments.

Do not rewrite logic.

### Acceptance checks

```text
- Empty anonymous namespace blocks removed.
- Active comments describe current code, not a past refactor state.
```

---

# Docs pass

## Pass 8 — Active profile status in docs

### Required current status

Active docs must classify profiles as:

```text
TonalPulse         stable active
ChirpExperimental selectable experimental / developer-only
AmpStateProfile   roadmap-only
```

### Required changes

Update active docs/manual/current pass/status docs accordingly.

Do not claim:

```text
AmpStateProfile is implemented/current
Chirp is stable
profile=chirp is valid
```

Use:

```text
profile=tonalpulse
profile=chirp_experimental
```

Do not use:

```text
profile=chirp
```

### Acceptance checks

```text
- Normal quickstart/manual shows only TonalPulse.
- Developer/experimental section may show ChirpExperimental.
- AmpState appears only as roadmap/proof-profile future work.
- profile=chirp is not documented as valid.
```

---

## Pass 9 — Add AMP diagnostic sunset condition to docs

### Required wording

Add a short note to the active current-pass or implementation-status doc:

```text
AmpDiagnosticProbe is transitional Analyzer-only diagnostic support.
It may be used in AMPDIAG / SEQ_EXPLAIN to explain amplitude-side observations.
It must not affect PatternResult, FieldState, Behavior, or primary SEQ classification.

Sunset condition:
Remove AmpDiagnosticProbe from Analyzer once SEQ_EXPLAIN can explain TonalPulse misses through native pipeline facts:
- candidateAccepted
- patternMatched
- supportMatched
- valid
- pattern reject reason
- ampSupport
- ampWindow availability
- frequency score / contrast
```

### Acceptance checks

```text
- Docs clearly mark AmpDiagnosticProbe as transitional.
- Docs do not present AMP diagnostic as active detection truth.
```

---

# Final acceptance checklist

The pass is complete when:

```text
[x] Analyzer keeps AmpDiagnosticProbe only as Analyzer AMPDIAG / SEQ_EXPLAIN support.
[x] Analyzer no longer depends directly on AmpTransientDetector::TransientRejectReason.
[x] AMP diagnostic reasons are printed as ampdiag_reason/details, not primary SEQ result names.
[x] currentTrialHit is removed.
[x] RB DETECT branch is removed completely.
[x] detectOnly=0 is removed from RB status.
[x] unused kLiveFrequency* constants/comments are removed or renamed if actually used.
[x] Small stale comments/empty namespaces are cleaned.
[x] Active docs say TonalPulse stable, ChirpExperimental experimental, AmpState roadmap-only.
[x] Manual/normal quickstart exposes only TonalPulse.
[x] Developer/experimental section may expose profile=chirp_experimental.
[x] No plain profile=chirp alias or documentation remains.
[x] AmpDiagnosticProbe sunset condition is documented.
```

## Suggested smoke checks

After code changes, run at least:

```text
RB STATUS
RB PROFILE name=tonalpulse
RB PROFILE name=chirp
RB PROFILE name=chirp_experimental
SEQ help
SEQ start tries=3 profile=tonalpulse
SEQ start tries=3 profile=chirp_experimental
```

Expected:

```text
RB PROFILE name=chirp -> error / suggest chirp_experimental
RB PROFILE name=chirp_experimental -> selects experimental profile if supported
Normal help/manual -> TonalPulse stable only
SEQ_TRIAL primary result remains generic
SEQ_EXPLAIN may include ampdiag fields
```
