# ResonantNode Cleanup / Bugfix Instruction v3

Scope: ResonantNode / Resonanzraum firmware cleanup after updated Architecture Spec v0.2.3, Detection Roadmap v0.4, and Behavior Roadmap v0.1.

Intent: fix roadmap drift, finish half-transitions, and fix real bugs first. This is not a feature-expansion pass. Do not add a broad new framework. Do not preserve compatibility aliases unless explicitly required for a test command.

## 0. Core cleanup thesis

This cleanup is about turning the current transitional code into the intended architecture:

```text
AudioSignal
→ FeatureStreams / FeatureHistory
→ SignalEmitters / SignalDetectors
→ SignalCandidates
→ SignalInspector
→ InspectedSignals
→ PatternAssembler
→ PatternCandidates
→ PatternRules
→ PatternResults
→ Behavior
```

Parallel context path:

```text
FeatureStreams + SignalCandidates + InspectedSignals + PatternResults
→ FieldStateTracker
→ FieldState
→ Behavior
```

The cleanup should reduce ambiguity:

```text
old names + new names side by side        → one clean gate vocabulary
old detector truth + DetectionRuntime     → DetectionRuntime truth
fake profile labels                       → real fixed-stage profile config
Analyzer recomputing meaning              → Analyzer reporting decisions
Behavior reading detector internals       → Behavior consumes PatternResult + FieldState
```

## 1. Non-goals / non-items for this cleanup

Do **not** implement these now:

- no generic rule engine
- no virtual rule-object system
- no JSON/YAML/external profile registry
- no dynamic profile registry
- no broad BehaviorRuntime / OutputDispatcher / DebugReporter refactor yet
- no general CommandScheduler
- no complex action queues
- no full ChirpProfile implementation unless the profile apply points are already stable
- no WhiteNoiseRoom / WoodBlock / Chime / object-hit chains
- no large AnalyzerApp split before detection semantics are cleaned
- no compatibility aliases for old gate names

Do **not** flatten `DetectionProfile` into a few loose booleans. The updated roadmap wants a real but small profile composition model. Replace fake/decorative fields with stage-specific configs and fixed apply points.

Do **not** globally delete `AmpSignalEmitter`. Remove it from active `FreqAmpProfile` composition, but keep it available for `AmpStateProfile` / diagnostics.

Do **not** permanently delete `ChirpProfile` / `AmpStateProfile` as concepts. They are proof profiles. They must not affect FreqAmp behavior until profile composition is real.

## 2. Accepted / revised decisions

Use these decisions as scope constraints:

```text
D1  Runtime event timebase = wall-clock sample time.
D2  DetectionRuntime owns detection truth for Analyzer/Behavior.
D3  Analyzer hit gate = PatternResult.valid only.
D4  unexpected / duplicate / rejected are flags/counters, not primary result overrides.
D5  FreqAmpProfile = FrequencyMatch primary + AMP support inspection. No AmpSignalEmitter in FreqAmp.
D6  Missing AMP support history/window = unknown/failed support by default. No baseline=0 pass.
D7  Own-emission suppression gates detection ingestion, not only behavior response.
D8  FreqAmp is the only stable active runtime profile for this cleanup. AmpState/Chirp are parked proof profiles.
D9  FreqAmp uses SinglePulseOnly PatternAssembler. Chirp may later enable multi-signal grouping.
D10 DetectionProfile keeps only fields/configs applied at fixed runtime stages.
D11 RB PARAM never resets behavior config. Profile reset must be explicit/logged.
D12 RB PARAM exposes active runtime tuning only. Old onset/release/cooldown move to AMPDIAG or are removed.
D13 externalEmitter uses explicit wider windows, not infinite in-window.
D14 Rejected signal candidates do not become PatternResults. They may appear in SEQ_EXPLAIN diagnostics.
D15 FieldState may track raw activity separately, but Behavior must not confuse raw density with valid pattern activity.
```

Revised D10 detail:

```cpp
struct DetectionProfile {
    InspectionConfig inspection;
    PatternRulesConfig patternRulesConfig;
    BehaviorGateConfig behaviorGate;
    FieldStateConfig fieldState;

    SignalEmitterSelection emitters;
    ProfileInspectionRulesKind inspectionRules;
};
```

Fixed runtime apply points:

```text
DetectionRuntime/profile factory applies emitter selection.
SignalInspector applies InspectionConfig and inspectionRules.
PatternRules applies PatternRulesConfig.
Behavior applies BehaviorGateConfig.
FieldStateTracker applies FieldStateConfig.
```

## 3. Required clean gate vocabulary

Use exactly this gate chain:

```text
candidateAccepted  → SignalInspector
patternMatched     → PatternRules
supportMatched     → PatternRules for FreqAmp support validity
behaviorEligible   → Behavior
```

Remove/replace old or ambiguous terms:

```text
candidateValid                 → candidateAccepted
tonalValid                     → patternMatched
conditionMatched               → remove if duplicate of patternMatched
requireTonalForBehavior        → PatternRulesConfig.requireSupportForAcceptance / BehaviorGateConfig
LocalityClass / near / mid/far → AmpSupportClass
```

No old/new aliases. No compatibility layer.

## 4. Pass order

Implement in this order. Each pass should compile and run smoke checks before the next pass.

---

# Pass 1 — Timing correctness and sample-time cleanup

## Goal

Make runtime timing trustworthy before changing detection semantics.

## Items

### 1.1 Verify `AudioSourceI2S::begin()` success logic

Current suspicious pattern:

```cpp
const int beginResult = I2S.begin(...);
if (beginResult != 0) {
    // success path
} else {
    _started = false;
}
```

Do not blindly invert this. Arduino-ESP32 `I2S.begin()` likely returns nonzero/true on success. Make the logic explicit, but preserve the correct success convention.

Acceptance:

```text
I2S success/failure handling is readable.
No change that marks successful I2S begin as failed.
```

### 1.2 Fix `AudioBlock.approxStartMicros`

`AudioBlock.approxStartMicros` must mean the approximate wall-clock time of the first sample returned in the block.

Do not set it to `fillEndUs`.

Do not use only read batch duration if I2S backlog exists. Estimate the age of the first returned sample using `availableBytes` captured before the read.

Preferred model:

```cpp
const uint32_t availableSamples = availableBytes / bytesPerSample;
const uint32_t readSamples = bytesRead / bytesPerSample;

// If I2S.read returns oldest available samples, first returned sample is
// approximately the oldest currently available sample.
const uint32_t firstReturnedSampleAgeSamples =
    availableSamples > 0 ? availableSamples - 1 : readSamples > 0 ? readSamples - 1 : 0;

_blockApproxStartMicros =
    fillEndUs - sampleOffsetUs(firstReturnedSampleAgeSamples, sampleRateHz);
```

Use 64-bit multiplication before division to avoid integer-period drift:

```cpp
static uint32_t sampleOffsetUs(uint32_t sampleOffset, uint32_t sampleRateHz) {
    return static_cast<uint32_t>((static_cast<uint64_t>(sampleOffset) * 1000000ULL) / sampleRateHz);
}
```

Acceptance:

```text
block.approxStartMicros represents first returned sample time.
Backlog is at least approximately accounted for.
No block end timestamp is consumed as block start timestamp.
```

### 1.3 Make `sampleTimeUs` wall-clock sample time

In both Node and Analyzer manual block loops, pass wall-clock sample time into `_audioSignal.update(...)`.

Do not pass synthetic sample-index time as runtime `sampleTimeUs`.

Target:

```cpp
const uint32_t sampleOffsetUs = sampleOffsetUs(i, sampleRateHz);
const uint32_t sampleTimeUs = block.approxStartMicros + sampleOffsetUs;

AudioSignalFrame frame;
_audioSignal.update(sample, sampleTimeUs, frame);
```

Avoid post-patching `frame.sampleTimeMs` after update. `AudioSignal::update()` should derive frame time from the wall-clock `sampleTimeUs`.

Acceptance:

```text
AudioSignalFrame.sampleTimeUs and sampleTimeMs describe the same wall-clock sample time.
No runtime event path compares sample-index pseudo-time to millis() windows.
```

### 1.4 Fix `FrequencyEvidence.observedAtMs`

Add explicit timestamp input:

```cpp
FrequencyEvidence captureFrequencyEvidence(unsigned long observedAtMs) const;
```

Runtime call sites must use:

```cpp
captureFrequencyEvidence(frame.sampleTimeMs);
```

Do not silently default runtime evidence to `millis()`.

Acceptance:

```text
FrequencyEvidence.observedAtMs is sample/frame time for runtime detection.
Diagnostic-only paths may use millis() only if explicitly named diagnostic.
```

### 1.5 Wire `FreqBandStream` to active target and sample rate

Ensure `FreqBandStream` uses the active tone/chirp/test frequency and actual audio sample rate.

At minimum:

```cpp
_freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
_freqBandStream.setTargetFrequencyHz(activeToneHz);
```

For Analyzer SEQ, use `_sequenceTest.toneHz` or equivalent active test tone.

If the target frequency changes, reset frequency stream state to avoid mixed-frequency windows.

Acceptance:

```text
Tests at non-3200 Hz analyze the selected frequency, not a hardcoded default.
```

Status: DONE

---

# Pass 2 — SEQ / Analyzer timing correctness

## Goal

Make Analyzer trial windows and classifications trustworthy.

## Items

### 2.1 Use scheduled trigger time for trial windows

Do not let loop lag move the expected window.

Target:

```cpp
scheduledAtMs = _sequenceTest.nextTriggerAtMs;
windowStartMs = scheduledAtMs + windowStartOffsetMs;
windowEndMs   = scheduledAtMs + windowEndOffsetMs;
```

Also store/log:

```text
loopLagMs = now - scheduledAtMs
commandSentAtMs = actual local emitter command send time, if applicable
```

If local emitter command-send time is chosen as physical truth, store it explicitly and use `expectedTriggerMs` consistently. Do not mix scheduled and actual send time silently.

Acceptance:

```text
SEQ dt and window classification use one explicit expectedTriggerMs.
Loop lag is visible in logs.
```

### 2.2 Fix external emitter window

Remove logic equivalent to:

```cpp
inWindow = externalEmitter || (...)
```

External emitter must use an explicit wider window, not an infinite whole-period window.

Acceptance:

```text
externalEmitter still allows wider timing uncertainty.
Noise anywhere in the trial period is not automatically in-window.
```

### 2.3 Fix `unexpected` semantics

Unexpected must not override a valid expected/late hit.

Target:

```text
primaryResult = expected | early | late | miss | rejected
flags/counters = unexpected_count, duplicate_count, rejected_count
```

Acceptance:

```text
A trial with a valid expected hit plus extra unexpected candidate remains expected with unexpected_count > 0.
```

### 2.4 Remove fake queue diagnostics

If `queue_before` is always `0`, remove it or compute it for real.

Acceptance:

```text
Analyzer logs do not include fake diagnostics.
```

Status: DONE

---

# Pass 3 — Clean gate vocabulary switch

## Goal

Apply the new gate vocabulary cleanly and delete old ambiguous names.

## Items

### 3.1 Rename / replace old fields

Replace:

```text
candidateValid  → candidateAccepted
tonalValid      → patternMatched
conditionMatched → remove if duplicate
LocalityClass / near / mid / far → AmpSupportClass
```

Do not keep aliases.

### 3.2 Define gate ownership in code comments / types

- `candidateAccepted`: produced by `SignalInspector`
- `patternMatched`: produced by `PatternRules`
- `supportMatched`: produced by `PatternRules` for FreqAmp AMP support validity
- `behaviorEligible`: produced by `Behavior`

### 3.3 Update logs and Analyzer output

Analyzer should print the gate chain:

```text
candidateAccepted=...
patternMatched=...
supportMatched=...
behaviorEligible=...
reason=...
```

Acceptance:

```text
grep finds no old gate names except in archived docs/tests if intentionally preserved.
Analyzer and debug output show the stage chain.
```

Status: DONE

---

# Pass 4 — Minimal real DetectionProfile composition

## Goal

Replace fake/decorative profile fields with real stage-specific configs.

## Items

### 4.1 Define stage configs

Use plain structs. No generic rule engine.

What we have already trimmed away from the profile shell:
- `featureSet`
- `signalDetector`
- `frequencyOnly`
- `ampEnabled`
- decorative `patternAssembler` labels

What the active `DetectionProfile` currently keeps as runtime-relevant composition:
- `kind`
- `signalEmitter`
- `inspectionRules`
- `patternRules`
- `patternRulesConfig`
- `inspectionConfig`
- `fieldStateConfig`

What we still intend to add as real stage-owned payloads:
- `BehaviorGateConfig`

```cpp
struct DetectionProfile {
    InspectionConfig inspection;
    PatternRulesConfig patternRulesConfig;
    BehaviorGateConfig behaviorGate;
    FieldStateConfig fieldState;
    SignalEmitterSelection emitters;
    ProfileInspectionRulesKind inspectionRules;
};
```

### 4.2 Fixed apply points

Implement/apply configs only at these points. Some of these are already live; the rest should stay as the next real apply points rather than decorative fields:

```text
DetectionRuntime     <- signalEmitter selection
SignalInspector      <- inspectionRules + InspectionConfig
PatternRules         <- PatternRulesConfig
Behavior             <- BehaviorGateConfig
FieldStateTracker    <- FieldStateConfig
```

### 4.3 Delete loose/decorative fields

Remove old fields if they only describe a future composition and do not configure runtime.

Do not replace them with another descriptive enum unless it is actually applied.

Acceptance:

```text
DetectionProfile fields configure runtime or are gone.
No decorative profile fields remain.
No JSON/YAML/registry/rule engine introduced.
```

Status: DONE

---

# Pass 5 — FreqAmpProfile real composition

## Goal

Make FreqAmpProfile the one stable active runtime profile for this cleanup.

## FreqAmpProfile definition

```text
Primary candidate source: FrequencyMatch / FrequencySignalEmitter
AMP role: support inspection through FeatureHistory / ScalarWindow
AmpSignalEmitter: disabled for FreqAmp
PatternAssembler: SinglePulseOnly
PatternRules: patternMatched from frequency pattern; supportMatched from amp_support gate
valid = patternMatched && supportMatched
```

## Items

### 5.1 Disable AmpSignalEmitter in FreqAmp

Do not delete `AmpSignalEmitter` globally.

For FreqAmp:

```cpp
enableFrequencySignalEmitter = true;
enableAmpSignalEmitter = false;
enableAmpSupportInspection = true;
```

### 5.2 Use AmpSupportClass directly

Use:

```text
unknown
none
weak
medium
strong
```

Remove near/mid/far locality mapping from active code.

### 5.3 No AMP history fallback pass

If retrospective AMP support window is missing/unavailable:

```text
amp_support = unknown
supportMatched = false
reason = missing_amp_support
```

Do not use baseline `0` as a fake valid support baseline.

### 5.4 PatternRules owns FreqAmp support gate

For FreqAmp:

```text
patternMatched=true
supportMatched=false
valid=false
reason=amp_support_too_low | missing_amp_support
```

Weak/None/Unknown become rejected/residual results with explicit reasons. They are visible to Analyzer but not valid behavior inputs.

### 5.5 SinglePulseOnly assembler

FreqAmp must not emit PulseSequence candidates.

For the current runtime, the assembler stays fixed as single-pulse only:

```text
single_pulse_only
```

Acceptance:

```text
FreqAmp emits frequency-derived single-pulse PatternResults only.
AMP support is inspection evidence, not an independent candidate source.
Weak/missing AMP support is reported as rejected/residual, not valid.
```

Status: DONE

---

# Pass 6 — PatternResult and PatternRules contract

## Goal

Make PatternResult the meaning-bearing output and prevent Analyzer/Behavior from using the wrong gate.

## Items

### 6.1 `PatternResult.valid` rule

Only `PatternResult.valid == true` means valid detection result for Analyzer hit or Behavior input.

Do not count these as hits:

```text
candidate exists
candidate is in window
candidateAccepted == true
patternMatched == true but supportMatched == false
```

### 6.2 Confidence semantics

Do not report `confidence=1.0` for support-failed invalid results unless explicitly split.

Preferred:

```text
patternConfidence
supportConfidence
finalConfidence
```

Minimal fix:

```text
final confidence for reports follows valid/support state clearly.
```

### 6.3 Rejection reasons

Add explicit reasons where missing:

```text
missing_amp_support
amp_support_too_low
wrong_timing
signal_rejected
frequency_score_too_low
frequency_contrast_too_low
```

### 6.4 Rejected SignalCandidates do not become PatternResults

Rejected signal candidates may be exposed in `SEQ_EXPLAIN` as diagnostics, but they must not enter PatternAssembler as PatternResults.

Acceptance:

```text
PatternRules owns patternMatched/supportMatched/valid/reason.
Analyzer and Behavior use valid as hit gate.
Rejected signal candidates remain diagnostics.
```

Status: DONE

---

# Pass 7 — Analyzer correctness and reporting

## Goal

Analyzer reports the detection stage chain and trial outcome. It does not redo detection meaning.

## Items

### 7.1 Analyzer hit classification

Analyzer may classify trial outcome from:

```text
ExpectedEvent + PatternResult + timing window
```

It must not redo PatternRules or reinterpret low-level detector evidence into pattern meaning.

Hit gate:

```cpp
patternResult.valid == true
```

### 7.2 Typed classification from birth

Remove string-first logic such as:

```cpp
const char* result = "miss";
analyzerResultFromSequenceOutcome(result);
```

Use:

```cpp
AnalyzerClassification classification = classifyTrial(...);
```

Strings only at print time.

### 7.3 Active profile config visible

Analyzer output should include active profile summary:

```text
profile=freqamp
emitters=freq
ampSupport=enabled
ampSupportMin=medium
assembler=single_pulse
freqScoreMin=...
freqContrastMin=...
```

### 7.4 Gate chain visible

`SEQ_TRIAL` compact line should include key gates if space allows.

`SEQ_EXPLAIN` must include full chain:

```text
SIGNAL → INSPECTED → PATTERN_CANDIDATE → PATTERN_RESULT
candidateAccepted → patternMatched → supportMatched → behaviorEligible
```

### 7.5 Summary accounting

Fix:

```text
configured vs completed trial count
avg_dt for valid hits
avg_confidence for valid hits
unexpected_count / duplicate_count / rejected_count
```

Avoid invalid summaries such as:

```text
avg_dt=-1ms
avg_confidence=0.00 for valid runs
```

### 7.6 Memory / stack safety

Avoid large report buffers and stack-heavy report objects. Prefer streaming print and compact stored summaries.

Acceptance:

```text
Analyzer does not recompute detection meaning.
Analyzer shows profile config and gate chain.
SEQ_SUMMARY counts completed trials correctly.
No stack canary / unsafe huge buffers introduced.
```

Status: DONE

---

# Pass 8 — Behavior boundary only, no large behavior framework

## Goal

Enforce Behavior boundary without implementing the full behavior roadmap.

## Items

### 8.1 Behavior input boundary

Behavior consumes:

```text
PatternResult + FieldState + local timers/params/output status
```

Behavior must not read:

```text
SignalCandidate
InspectedSignal
FeatureStream
detector internals
raw AMP/frequency facts
```

### 8.2 Behavior owns behaviorEligible

Behavior computes/records:

```text
behaviorEligible
blockedReason
```

Based on:

```text
PatternResult.valid
FieldState
cooldown
self-suppression
refractory
probability
output busy status
```

### 8.3 Support-failed result handling

Support-failed PatternResults should not become vague `UnknownBlocked` logs. Report clear reason:

```text
ignored_invalid_pattern
ignored_missing_support
ignored_weak_support
blocked_self_suppressed
blocked_refractory
blocked_field_dense
```

### 8.4 Own-emission suppression gates detection ingestion

Use `ownEmitDetectionSuppressUntilMs()` before DetectionRuntime ingestion, not only inside Behavior response.

Do not let own emitted chirps pollute:

```text
DetectionRuntime
FieldState
candidate counters
idle/chatter/density
```

Acceptance:

```text
Behavior does not inspect detector internals.
Behavior emits/records behaviorEligible and blocked reason.
Own chirps are suppressed before detection ingestion where intended.
```

Do not implement full `BehaviorRuntime`, `OutputDispatcher`, or `DebugReporter` unless required by the smallest local change.

Status: DONE

---

# Pass 9 — FieldState boundary and minimal cleanup

## Goal

Keep FieldState as acoustic context, not pattern meaning.

## Items

### 9.1 FieldState is not PatternResult

Do not create fake PatternResults like:

```text
BUSY_ROOM
CHATTER
QUIET
```

### 9.2 FieldState does not drive PatternRules

PatternRules must not use FieldState to classify pattern validity.

### 9.3 Separate raw activity from valid pattern activity

If current FieldState mixes raw candidates and valid pattern density, at minimum make this clear in fields/logs:

```text
recentSignalCount
recentAcceptedSignalCount
recentPatternCount
recentValidPatternCount
rawActivity
validPatternActivity
```

Near-term priority: disabling AmpSignalEmitter in FreqAmp should reduce FieldState pollution. Do not overbuild FieldState in this cleanup.

Acceptance:

```text
FieldState remains context.
Behavior may use FieldState for response policy.
PatternRules do not classify from FieldState.
```

Status: DONE

---

# Pass 10 — Commands, params, and help text

## Goal

Make serial commands reflect the active architecture.

## Items

### 10.1 RB PARAM exposes active runtime tuning only

`RB PARAM` should not expose old/direct AMP detector knobs if they do not tune active runtime.

Current active FreqAmp tuning should be things like:

```text
freqScore
freqContrast
ampSupport thresholds/basis
field/behavior gates if intentionally exposed
```

Move old onset/release/cooldown to:

```text
RB AMPDIAG ...
```

or remove them from Node runtime mode.

### 10.2 RB PARAM must not reset behavior config

Split apply functions:

```cpp
applyActiveDetectionProfile();
applyActiveBehaviorGateConfig();
```

`RB PARAM` must not call a broad `applyActiveProfiles()` that resets behavior overrides.

Profile switch may reset defaults only if explicit/logged.

### 10.3 Help text must match reality

Remove misleading command/help text such as:

```text
detector=AMP
profile=freqamp|chirp if chirp is not active
RB DETECT removed; use RB PROFILE
```

Active help should show:

```text
profile=freqamp
freq tuning
amp support config
AMPDIAG if diagnostic AMP detector exists
SEQ_TRIAL / SEQ_EXPLAIN / SEQ_SUMMARY
RAW_SAMPLE_CAPTURE separate
```

### 10.4 Remove old aliases

Final stable names:

```text
SEQ_TRIAL
SEQ_EXPLAIN
SEQ_SUMMARY
RAW_SAMPLE_CAPTURE
```

Remove old aliases if still present:

```text
raw
raw_debug
liveraw
freq_class
trialbrief
triallite
brief
report
SEQ_REPORT*
SEQ_EXPLAIN_LEGACY_*
SEQ_LEGACY_PROFILE_SUMMARY
```

Acceptance:

```text
Serial help reflects actual active runtime.
No command exposes inactive tuning as if active.
RB PARAM does not reset behavior config.
```

Status: DONE

---

# Pass 11 — Code cleanup / dead paths

## Goal

Delete code that is now clearly stale after semantic cleanup.

## Items

### 11.1 Remove obvious repo/source garbage

Do not include in review ZIP/repo:

```text
.pio/
src.zip
src (2).zip
```

### 11.2 Remove stale demo files from `src`

Remove or move outside active `src`:

```text
src/behavior/BlinkBehavior.*
src/behavior/ButtonLedBehavior.*
src/hal/Board.*
```

If kept as examples, move under `examples/` and fix case-sensitive includes.

### 11.3 Remove `node.cpp` dead code

Delete:

```text
#if 0 logCandidate block
unused H3 helpers
unused live-frequency constants
unused emittedPattern variable
unused stats variables
```

### 11.4 Centralize defaults

Centralize default tone/frequency constants.

Example:

```cpp
constexpr uint32_t kDefaultChirpFrequencyHz = CHIRP_FREQUENCY_HZ;
constexpr uint32_t kDefaultChirpDurationMs = 100;
```

Replace hardcoded `3200` in runtime code with active profile/output defaults.

### 11.5 Remove fake/unreachable output phases

If `ChirpOutput` phases 2/3 and `_idleChirpLongPauseMs` are unreachable, delete or rewrite the state machine intentionally.

### 11.6 Cleanup duplicate formatters

Collapse duplicated `SEQ_CAND` print blocks into one formatter after gate vocabulary is stable.

Acceptance:

```text
Dead code removed.
No obvious stale demo files in active source.
Runtime defaults have one source of truth.
```

Status: DONE

---

# Pass 12 — Mechanical Analyzer split, only after semantics are clean

## Goal

Reduce `AnalyzerApp.cpp` size after timing, gates, and profile semantics are stable.

Do not do this before Passes 1–10, or the confused semantics will be spread across more files.

Target extraction:

```text
AnalyzerApp              setup + loop order only
AnalyzerCommands         serial parsing
AnalyzerEmitterControl   trigger/chirp commands
AnalyzerRawCapture       RAW_SAMPLE_CAPTURE
AnalyzerSequenceRunner   SEQ lifecycle
AnalyzerClassifier       ExpectedEvent + PatternResult → AnalyzerClassification
AnalyzerReports          SEQ_TRIAL / SEQ_EXPLAIN / SEQ_SUMMARY printing
```

Rule:

```text
Move code first.
Do not rewrite behavior during the split.
```

Acceptance:

```text
AnalyzerApp is coordinator-sized.
SEQ behavior unchanged after split.
Report semantics already cleaned before extraction.
```

Status: DONE

---

# 5. Smoke checks after key passes

After Pass 1 / timing:

```text
- Audio source starts successfully.
- Logged sample/frame times increase monotonically.
- No frame has sampleTimeUs/sampleTimeMs from different clocks.
- FreqBandStream reports selected target frequency.
```

After Pass 3 / gates:

```text
- Analyzer output contains candidateAccepted/patternMatched/supportMatched/behaviorEligible.
- No old candidateValid/tonalValid/LocalityClass active names remain.
```

After Pass 5 / FreqAmp:

```text
- FreqAmp emits frequency-derived single-pulse PatternResults.
- AmpSignalEmitter is not active in FreqAmp.
- AMP support inspection runs and reports unknown/none/weak/medium/strong.
- Missing AMP history does not pass support.
```

After Pass 7 / Analyzer:

```text
- Valid PatternResult in window → expected/late.
- supportMatched=false → rejected/residual, not expected hit.
- unexpected candidate does not override valid expected hit.
- SEQ_SUMMARY completed count and avg dt are sane.
```

After Pass 8 / Behavior:

```text
- Behavior consumes PatternResult + FieldState only.
- Self-emitted chirps do not enter DetectionRuntime during suppression window.
- Blocked reasons are explicit.
```

## 6. Final acceptance criteria

The cleanup is successful when:

```text
- Timing model is wall-clock sample-time based and internally consistent.
- DetectionRuntime is the active detection truth.
- Clean gate vocabulary is used with no compatibility aliases.
- FreqAmpProfile is a real fixed-stage composition.
- AmpSignalEmitter is disabled for FreqAmp but remains available for later AmpState/diagnostic use.
- PatternRules owns patternMatched/supportMatched/valid for FreqAmp.
- Behavior owns behaviorEligible.
- Analyzer reports the stage chain and does not recompute detection meaning.
- FieldState remains acoustic context, not pattern meaning.
- RB PARAM exposes only active runtime tuning and does not reset behavior config.
- Help text and logs match the real runtime.
- Dead/demo/legacy source has been removed or moved out of active src.
```

## 7. Implementation style rules for Codex

```text
Prefer deletion over compatibility.
Prefer fixed config structs over generic engines.
Prefer one owner per decision.
Do not add aliases for old names.
Do not mix active runtime and diagnostic paths.
Compile after each pass.
Keep behavior recognizable while detection cleanup is underway.
If a change requires broad behavior architecture work, stop and mark it deferred.
```
