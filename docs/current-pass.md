# Detection Stabilization Cleanup / Fix Plan

Scope: ResonantNode / TonalPulse detection stabilization after `myspec_v0.2.4_cleanup_candidate.md` and the follow-up code discussion.

Purpose: define the next cleanup/fix sequence around current detection instability, especially AMP instability vs FrequencyMatch stability.

This is **not** a broad architecture refactor. It is a stabilization plan.

---

## 0. Current Working Interpretation

Current observations:

```text
FrequencyMatch feels snappy and comparatively stable.
AMP inspection finds peaks, but weak/medium/strong class varies strongly.
AMP strength/locality is too unstable to be required TonalPulse support.
FrequencyScore / contrast are closer to the actual emitted tonal signal.
```

Likely causes of AMP instability include both acoustics and code/data-path issues:

```text
broad amplitude is not target-specific
absolute AMP thresholds are distance/mounting dependent
peak-only AMP classification is spike-sensitive
FeatureHistory currently risks same-ms overwrite
AMP history may use already gated `frame.level`
baseline tracking / quiet gate may affect AMP values
AMP/frequency windows may not align perfectly
```

Near-term decision:

```text
Do not keep AmpStrength as required TonalPulse support.
Move TonalPulse toward frequency-first validation:
FrequencyMatchSource + full-window strict FrequencyScoreStrength.
```

---

## 1. First Step: Preserve a Pre-Fix Baseline

Before changing low-level history, baseline, AMP, or thresholds:

```text
Run old/current TonalPulse exactly as-is.
```

Status:

```text
baseline running / captured on current pass
```

Use the already defined baseline plan:

```text
seq_baseline_old_tonalpulse_distance_ladder.md
```

Baseline ladder:

```text
30 cm
50 cm
70 cm
90 cm
120 cm
```

Prefer:

```text
100 trials per distance
```

Minimum if short on time:

```text
50 trials per distance
```

Save:

```text
SEQ_SUMMARY
SEQ_TRIAL lines
profile/status line
distance
node id
firmware/build label
physical notes
```

Do not change during baseline:

```text
thresholds
support target
baseline settings
history code
feature aggregation
output frequency/duration/gain
behavior mode
orientation/mounting
```

Reason:

```text
Without this baseline, later low-level fixes cannot be evaluated cleanly.
```

---

## 2. Stabilization Sequence Overview

Recommended implementation order:

```text
A — Pre-fix SEQ baseline
B — FeatureHistory same-ms aggregation
C — AMP envelope source cleanup: centered / ungated magnitude
D — ScalarFeatureStrength classification modes
E — Scalar inspection window timing modes
F — Repeat old TonalPulse baseline
G — New TonalPulse-B profile/config: FrequencyMatch + full-window strict FrequencyScoreStrength
H — AMP demotion / Analyzer output cleanup
I — Later only: baseline tracking diagnostics / slower baseline follow
```

Do not combine all of these with threshold tuning.

---

## 3. Pass A — Pre-Fix SEQ Baseline

Goal:

```text
Capture current old TonalPulse behavior before changing signal/history code.
```

Status:

```text
completed on current pass; baseline captured before low-level changes
```

Deliverables:

```text
baseline logs
SEQ_SUMMARY per distance
notes on AMP weak/medium/strong distribution
notes on freqScore/contrast trend
```

Success:

```text
Current miss/late/duplicate/reject behavior is known across distance.
```

Non-goal:

```text
No code changes.
```

---

## 4. Pass B — FeatureHistory Same-ms Aggregation

Problem:

```text
FeatureHistory is ms/time-bin based.
Multiple writes with the same timeMs can overwrite earlier values.
For AMP, this can lose peaks and make classification timing-dependent.
```

Status:

```text
implemented: same-ms feature writes now aggregate into per-ms bins
```

Fix:

```text
Keep FeatureHistory ms-based.
Change same-ms writes from overwrite to aggregation.
```

Per-bin target:

```text
timeMs
min
max
sum
count
last
```

ScalarWindow should derive:

```text
peak = max(bin.max)
mean = sum(bin.sum) / sum(bin.count)
last = newest bin.last
count/valueCount = sum(bin.count)
```

Do not make all FeatureHistory sample-based.

Reason:

```text
Sample-based FeatureHistory is RAM-heavy across multiple streams.
RawSampleHistory can remain sample-based for waveform diagnostics.
FeatureHistory should be ms-bin based but aggregated.
```

Success:

```text
FeatureHistory no longer loses same-ms AMP peaks.
ScalarWindow exposes peak and mean from aggregated bins.
```

Non-goals:

```text
No threshold retuning.
No profile behavior change.
No RawSampleHistory redesign.
```

---

## 5. Pass C — AmpEnvelope Uses Centered / Ungated Magnitude

Problem:

```text
FeatureExtractor currently records AmpEnvelope from frame.level.
If frame.level is quiet-gated or smoothed, AMP inspection may see a processed value instead of raw centered magnitude.
```

Fix:

Add/expose:

```text
centeredMagnitude = abs(raw - baseline)
```

Then write:

```text
FeatureStreamId::AmpEnvelope = centeredMagnitude
```

Keep existing `frame.level` for current logic unless explicitly needed elsewhere.

Diagnostic output should allow comparison:

```text
amp.centeredMagnitude
amp.level
amp.baseline
```

Success:

```text
AMP inspection history receives ungated centered magnitude.
Quiet gate effects can be distinguished from actual weak AMP.
```

Non-goals:

```text
No baseline rewrite.
No baseline alpha change.
No quiet threshold retuning.
```

---

## 6. Pass D — ScalarFeatureStrength Classification Modes

Problem:

```text
Scalar inspection is too close to peak-only absolute classification.
For AMP this is unstable.
For FrequencyScore, full-window mean/strict classification is needed.
```

Add explicit mode enum:

```cpp
enum class ScalarInspectionMode {
    PeakAbsolute,
    MeanAbsolute,
    SustainedAboveThreshold,
};
```

Scope:

```text
Applies to ScalarFeatureStrength in the inspector path only.
Does not change ScalarOccurrenceSource behavior.
```

Optional later, not required now:

```text
PeakLift
MeanLift
```

Do **not** implement lift/local-floor modes until a reliable floor-window model exists.

### Mode semantics

```text
PeakAbsolute:
    legacy-compatible mode
    classificationValue = window.peak

MeanAbsolute:
    full-window / stable support mode
    classificationValue = window.mean

SustainedAboveThreshold:
    spike rejection mode
    classify based on bins/ms above threshold
```

Extend `ScalarFeatureInspectionConfig` with:

```text
mode
minSustainedMs or minSustainedCount
```

Do not retune thresholds.

Analyzer/SEQ_EXPLAIN should expose:

```text
scalar.mode
scalar.peak
scalar.mean
scalar.last
scalar.count
scalar.classificationValue
scalar.strengthClass
```

Success:

```text
PeakAbsolute preserves legacy behavior.
MeanAbsolute is available for full-window FrequencyScore validation.
SustainedAboveThreshold is available or explicitly deferred.
```

Status:

```text
implemented in inspector path only; ScalarOccurrenceSource is unchanged
```

---

## 7. Pass E — Scalar Inspection Window Timing Modes

Problem:

```text
AMP around frequency and full-window frequency validation need explicit window semantics.
```

Add minimal explicit window mode:

```cpp
enum class ScalarInspectionWindowMode {
    FullOccurrence,
    CustomOffsets,
};
```

Optional later:

```text
EarlyOccurrence
PeakCentered
```

Semantics:

```text
FullOccurrence:
    start = occurrence.startMs
    end   = occurrence.endMs

CustomOffsets:
    start = occurrence.startMs + preMs
    end   = occurrence.endMs + postMs
```

Use signed offsets if supported.

Recommended future usage:

```text
FrequencyScoreStrength:
    mode = MeanAbsolute
    windowMode = FullOccurrence

AmpStrength diagnostic:
    mode = PeakAbsolute or SustainedAboveThreshold
    windowMode = FullOccurrence or CustomOffsets
```

Success:

```text
Scalar inspection windows are explicit rather than implicit.
```

Non-goal:

```text
No arbitrary window expression system.
```

---

## 8. Pass F — Repeat Old TonalPulse Baseline

After passes B–E, repeat the same old/current TonalPulse distance ladder.

Do not tune thresholds yet.

Compare against pre-fix baseline:

```text
miss rate
late rate
duplicate rate
rejected count
AMP weak/medium/strong distribution
freqScore support distribution
avg_dt
avg_dur
```

Purpose:

```text
Measure what changed due to low-level history/evidence mechanics only.
```

---

## 9. Pass G — TonalPulse-B Profile / Config

After low-level mechanics are stable, test the new frequency-first support model.

Target profile/config:

```text
OccurrenceSource:
    FrequencyMatch

Inspection:
    ScalarFeatureStrength
    stream = FrequencyScore
    target = FrequencyScoreStrength
    mode = MeanAbsolute
    windowMode = FullOccurrence

PatternRules:
    requireSupportForAcceptance = true
    requiredSupportTarget = FrequencyScoreStrength

AMP:
    optional diagnostic only
    not required support
```

Interpretation:

```text
FrequencyMatchSource catches likely tonal occurrences.
Full-window FrequencyScoreStrength validates them strictly.
AMP no longer gates TonalPulse acceptance.
```

Important guardrail:

```text
FrequencyMatchSource should be permissive enough to catch candidates.
FrequencyScoreInspector should be the stricter full-window validation.
Do not make both gates identical.
```

Test matrix:

```text
A — FrequencyMatch only
    supportRequired = false

B — FrequencyMatch + full-window strict FrequencyScoreStrength
    supportRequired = true
    requiredSupportTarget = FrequencyScoreStrength

C — old/current AMP-supported TonalPulse
    supportRequired = true
    requiredSupportTarget = AmpStrength
```

Expected:

```text
A = sensitivity baseline
B = likely best useful balance
C = confirms AMP instability
```

---

## 10. Pass H — AMP Demotion / Analyzer Output Cleanup

If AMP remains unstable:

```text
do not show AMP as central default SEQ_TRIAL truth
```

Default `SEQ_TRIAL` for TonalPulse-B should emphasize:

```text
profile
source
result
pattern
dt
duration
freqScore.full / mean
freqScore.supportClass
freqContrast / peak contrast
reason
```

Move AMP to:

```text
SEQ_EXPLAIN
optional diagnostic output
separate AMP diagnostic mode
```

Do not remove AMP code completely yet; demote its role.

---

## 11. Later Pass I — Baseline Tracking Diagnostics / Slow Baseline

Baseline tracking is a suspect, but should be measured before changing.

Add diagnostic fields:

```text
raw
baseline
centered
centeredMagnitude
levelAfterGate
baselineDelta
baselineUpdatedThisFrame
```

Only after evidence:

```text
make baseline following slower
freeze baseline during active/high-energy windows
separate DC removal from quiet gating
```

Do not change baseline tracking in the same pass as FeatureHistory aggregation or threshold tuning.

---

## 12. Code Cleanup Items in Parallel / After Stabilization

Useful cleanup after or between stabilization passes:

```text
PatternAssembler:
    acceptSignal → acceptOccurrence
    _recentSignals → _recentOccurrences

FieldState:
    busySignalCountThreshold → busyOccurrenceCountThreshold
    denseSignalCountThreshold → denseOccurrenceCountThreshold
    quietSignalCountThreshold → quietOccurrenceCountThreshold

PatternResultKind:
    ValidChirp → Valid
    InvalidChirp → Invalid

Generic scalar inspector naming:
    annotateAmpStrength → annotateScalarFeatureStrength
    AmpStrengthEvidence → ScalarStrengthEvidence or target-specific evidence
```

These are mostly naming/contract cleanup, not detection logic.

---

## 13. Non-Goals for This Stabilization Plan

```text
No pulsed chirp grouping.
No CandidateCorrelator.
No behavior architecture refactor.
No OutputDispatcher.
No ParamRegistry.
No VEKTOR exposure.
No broad profile factory rewrite.
No runtime graph-builder config.
No threshold retuning mixed into low-level fixes.
```

Status:

```text
implemented: AmpEnvelope now records centeredMagnitude, and analyzer/inspector diagnostics print centered values explicitly
```

---

## 14. Recommended Immediate Next Step

```text
Run the old TonalPulse SEQ distance ladder baseline.
```

Then implement:

```text
FeatureHistory same-ms aggregation
AmpEnvelope centered / ungated magnitude
ScalarInspectionMode
ScalarInspectionWindowMode
```

Then repeat baseline.

Only after that:

```text
test TonalPulse-B:
FrequencyMatch + full-window strict FrequencyScoreStrength
```

---

## One-Line Plan

```text
Baseline old TonalPulse first, fix low-level scalar/history mechanics without retuning, repeat the ladder, then test frequency-first TonalPulse-B with full-window strict FrequencyScore support and AMP demoted to diagnostics.
```
