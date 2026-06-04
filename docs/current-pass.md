# ResonantNode Detection Cleanup Package — Codex Action Plan

## Scope

Actionable implementation checklist for Codex.

Preserve the same decisions:

1. Compatibility sediment must be removed before larger cleanup.
2. Analyzer reporting must become parity-based for Scalar and Frequency sources.
3. Frequency / AMP feature flow must use parallel fan-out into FeatureHistory and OccurrenceSource.
4. FrequencyMatch must be cleaned up while staying parallel to ScalarDetector.
5. Naming cleanup must make Value / Sample / Packet / Window / Frame / Evidence / Label explicit.
6. Miss-streak diagnostics must classify bad regimes rather than tuning blindly.

## Non-goals

Do **not** do these in this package:

- Do not tune thresholds.
- Do not add Hann / 3-bin / FFT / spectral-scan behavior except diagnostic-only miss scan where explicitly requested.
- Do not redesign DetectionRuntime architecture.
- Do not rewrite Node.
- Do not change profile semantics.
- Do not rename archive docs.
- Do not make FeatureHistory the live pipe into OccurrenceSource.
- Do not remove useful profile-specific diagnostics; move them under namespaced detail blocks.

---

# Commit Instructions

## Commit format

Use this exact subject format:

```text
DetectionCleanup [NN-NN] <short imperative summary>
```

Where:

```text
NN-NN = pass number and optional subitem number
```

Examples:

```text
DetectionCleanup [00-01] Record Analyzer baseline
DetectionCleanup [01-01] Fix duplicate primary classification
DetectionCleanup [02-05] Add shared source diagnostics printer
DetectionCleanup [03-05] Gate FrequencyMatch on fresh measurements
DetectionCleanup [04-03] Rename frequency feature frame
DetectionCleanup [05-01] Add miss fault fingerprint output
```

For whole-pass commits, use:

```text
DetectionCleanup [NN] <short imperative summary>
```

Examples:

```text
DetectionCleanup [01] Remove compatibility sediment
DetectionCleanup [02] Add scalar frequency reporting parity
```

Prefer subitem commits when the change is risky or reviewable alone.

## Commit body

Every commit body should include:

```text
Why:
- ...

Changed:
- ...

Checks:
- ...
```

Optional section when behavior may change:

```text
Behavior:
- ...
```

Optional section when follow-up is known:

```text
Follow-up:
- ...
```

Example:

```text
DetectionCleanup [01-02] Unify frequency evidence classification

Why:
- SEQ_TRIAL and SEQ_SUMMARY used separate frequency evidence classifiers.
- Summary counts could diverge from printed trial classes.

Changed:
- Added one classifyFrequencyEvidence(...) helper.
- Reused it for trial report and summary counters.

Checks:
- Build passes.
- SEQ_TRIAL freq class and SEQ_SUMMARY counts use the same enum/index mapping.

Behavior:
- Reporting/statistics only. Detection thresholds unchanged.
```

## Commit frequency

Commit after each subitem when:

```text
- the code compiles
- Analyzer smoke output is still readable
- no unrelated tuning or architecture rewrite slipped in
```

Do not batch unrelated changes into one commit. Keep reporting fixes, behavior-gating changes, and broad renames separate.

## Commit guardrails

Before each commit, check:

```text
git diff --stat
git diff
```

Verify:

```text
- no threshold tuning
- no profile semantic changes
- no archive docs edited
- no broad rename mixed with behavior change
- no DetectionRuntime redesign unless explicitly scoped
```

---

# 00 — Preflight

## 00.1 Create branch

Action:

```text
git checkout -b refactor/detection-cleanup-reporting-parity
```

Commit:

```text
No commit required unless project policy commits branch notes/docs.
```

Acceptance:

```text
Clean branch exists.
Current code compiles before edits.
```

## 00.2 Record baseline

Action:

Run current Analyzer smoke test and save:

```text
startup output
SEQ_STATUS
one short SEQ run
current SEQ_SUMMARY
```

Commit:

```text
DetectionCleanup [00-02] Record Analyzer cleanup baseline
```

Body:

```text
Why:
- Establishes pre-cleanup Analyzer output for comparison.

Changed:
- Added/saved baseline output notes.

Checks:
- Current code compiles before cleanup.
- Baseline SEQ output captured.
```

Acceptance:

```text
There is a known pre-cleanup baseline for comparing output.
```

## 00.3 Add “do not tune” note

Action:

Add a short note to the top of the working issue / PR:

```text
This pass removes compatibility sediment and improves reporting truth.
No threshold tuning, DSP changes, or broad architecture rewrite.
```

Commit:

```text
DetectionCleanup [00-03] Add cleanup guardrails note
```

Acceptance:

```text
Reviewers/Codex context cannot confuse this with a detection tuning pass.
```

---

# 01 — Compatibility Sediment Fixes

Goal:

```text
Make current-main understandable.
Find hidden old/new behavior mixtures.
Prevent Analyzer/source classification from lying.
```

## 01.1 Fix duplicate-after-primary bug

Target area:

```text
AnalyzerSequenceHelpers.cpp
handleSequenceCandidate()
```

Problem:

The first valid in-window pattern sets `primaryValidPatternCaptured = true`, then enters the duplicate branch because the duplicate check uses the newly-set flag.

Action:

```cpp
const bool hadPrimaryBeforeThisCandidate = _sequenceTest.primaryValidPatternCaptured;

if (!hadPrimaryBeforeThisCandidate) {
    _sequenceTest.primaryValidPatternCaptured = true;
    _sequenceTest.primaryValidPattern = patternResult;
    _sequenceTest.primaryValidPatternDtMs = dtFromTriggerMs;
}

if (hadPrimaryBeforeThisCandidate) {
    // duplicate branch
}
```

Commit:

```text
DetectionCleanup [01-01] Fix duplicate primary classification
```

Acceptance:

```text
First valid in-window pattern does not increment duplicateCount.
DuplicateCount increases only for later valid patterns.
Expected trial classification is unchanged except corrected duplicate fields.
```

---

## 01.2 Unify frequency evidence classification

Target areas:

```text
AnalyzerApp.cpp
AnalyzerSequenceSession.cpp
AnalyzerReporting.*
```

Problem:

Two classifiers exist:

- Trial output assigns `report.frequency.freqEvidenceClass`.
- Summary uses `frequencyEvidenceClassIndex()` with different rules.

Action:

Create one canonical classifier:

```cpp
enum class FrequencyEvidenceClass {
    Accepted,
    StrongNoOccurrence,
    Partial,
    Weak,
    None
};

FrequencyEvidenceClass classifyFrequencyEvidence(const AnalyzerReport& report);
const char* frequencyEvidenceClassLabel(FrequencyEvidenceClass c);
size_t frequencyEvidenceClassIndex(FrequencyEvidenceClass c);
```

Use it for:

```text
report.frequency.freqEvidenceClass
SEQ_TRIAL print
_sequenceTest.freqEvidenceClassCounts
SEQ_SUMMARY labels/counts
```

Commit:

```text
DetectionCleanup [01-02] Unify frequency evidence classification
```

Acceptance:

```text
SEQ_TRIAL frequency evidence class and SEQ_SUMMARY frequency evidence counts use the same function.
No separate inference path remains.
```

---

## 01.3 Rename misleading `resetDetectorState()`

Target:

```text
AnalyzerApp.cpp / AnalyzerApp.h
```

Problem:

`resetDetectorState()` only resets `AudioSignal`, not DetectionRuntime or FrequencyMatch detector state.

Action:

Rename to:

```cpp
resetAudioSignalState()
```

or, if the function should reset real detection state, split explicitly:

```cpp
resetAudioSignalState()
resetDetectionRuntimeState()
```

Do not change behavior unless required by existing call intent.

Commit:

```text
DetectionCleanup [01-03] Rename audio signal reset helper
```

Acceptance:

```text
No function named resetDetectorState() only resets AudioSignal.
No runtime behavior change unless explicitly intended.
```

---

## 01.4 Mark or remove FrequencyMatch compatibility mirrors

Target:

```text
FrequencyMatchDetector.h
DetectionRuntime.cpp
```

Problem:

Fields marked “compatibility mirror” are still read as active diagnostic truth.

Examples:

```text
present
thresholdScore
thresholdContrast
readyOk
bestScoreOk
bestContrastOk
gateOpen
currentMatchRunFrames
```

Action, minimum:

```text
Remove misleading “compatibility mirror” comments if fields are still active API.
Add TODO(reporting-parity) marking them as temporary Analyzer diagnostic API.
```

Preferred action:

```text
DetectionRuntime diagnostics derive from explicit gate/lifecycle state, not mirror aliases.
```

Commit:

```text
DetectionCleanup [01-04] Clarify FrequencyMatch diagnostic mirrors
```

Acceptance:

```text
No active field is described as harmless compatibility-only while still driving Analyzer diagnostics.
```

---

## 01.5 Remove dead `#if 0` reporting block

Target:

```text
AnalyzerReporting.cpp
```

Action:

Delete old `#if 0` / disabled reporting code.

Commit:

```text
DetectionCleanup [01-05] Remove dead Analyzer reporting block
```

Acceptance:

```text
No old disabled SEQ_INSPECT block remains in active reporting file.
```

---

## 01.6 Support-gate reporting clarification

Target areas:

```text
PatternRules output
Analyzer report construction
AnalyzerReporting
```

Problem:

When `support_gate=disabled`, support target may still print as if it blocks acceptance.

Action:

When support gate is disabled, print:

```text
support_gate=disabled
required_support_target=diagnostic_only:<target>
```

Commit:

```text
DetectionCleanup [01-06] Clarify disabled support gate reporting
```

Acceptance:

```text
Output makes clear that support target is diagnostic-only when support gate is disabled.
No pattern acceptance behavior changes.
```

---

## 01.7 First-trial startup artifact flag

Target areas:

```text
Analyzer sequence session
AnalyzerReport classification
SEQ_SUMMARY aggregation
```

Problem:

First-trial `missing_pipeline_result` can be a startup/rebase/remote-claim artifact and should not pollute detector tuning stats.

Action:

Add a per-trial flag:

```cpp
bool startupArtifact = false;
```

Set it when:

```text
first trial
AND missing_pipeline_result
AND within startup/warmup/rebase context
```

Summary behavior:

```text
Count startup artifacts separately.
Do not include startupArtifact trials in detector miss tuning stats.
```

Commit:

```text
DetectionCleanup [01-07] Separate startup artifacts from miss stats
```

Acceptance:

```text
SEQ_SUMMARY reports startup_artifacts separately.
First-trial missing_pipeline_result is not mixed into detector miss statistics.
```

---

# 02 — Scalar / Frequency Reporting Parity

Goal:

```text
One shared source-stage reporting path.
FrequencyMatch and ScalarTransient remain different source types.
Profile-specific details move into namespaced detail blocks.
```

## 02.1 Inventory current report fields

Search:

```text
report.frequency.
report.scalar.
report.profileDetail.
report.occurrences.
inspectionObservations
printSequenceDiagnostics
printSequenceScalarDiagnostics
```

Action:

For each field, classify as one of:

```text
common source-stage truth
frequency-specific source detail
scalar-specific source detail
inspection detail
pattern detail
classification detail
debug/status only
```

Commit:

```text
DetectionCleanup [02-01] Map Analyzer report field ownership
```

Acceptance:

```text
A temporary mapping comment/table exists in code or PR notes.
No behavior change.
```

---

## 02.2 Introduce shared source report struct

Target:

```text
AnalyzerReport types
```

Action:

Add a common source-stage block:

```cpp
struct SourceStageReport {
    SourceKind sourceKind;
    const char* sourceName;

    bool acceptedPresent;
    bool sourceOccurrenceEmitted;
    bool runtimeEvidenceSeen;
    bool runtimeOccurrenceReceived;
    bool analyzerSeen;

    bool detectionGateBlocked;
    const char* detectionGateReason;

    SourceSummary sourceSummary;
    SourceCandidateSummary lastCandidate;

    bool activeAtTrialStart;
    bool activeAtTrialEnd;
    bool openedThisTrial;
    bool closedThisTrial;
    bool emittedThisTrial;
    bool rejectedThisTrial;

    FrequencyMatchSourceDetail frequencyMatch;
    ScalarTransientSourceDetail scalarTransient;
};
```

Temporary compatibility:

```text
Keep report.frequency and report.scalar while migrating.
Populate report.source as the new canonical reporting shape.
```

Commit:

```text
DetectionCleanup [02-02] Add shared source stage report
```

Acceptance:

```text
AnalyzerReport has one shared source-stage block.
Old fields still compile during transition.
```

---

## 02.3 Populate common source fields

FrequencyMatch path:

```text
sourceKind = FrequencyMatch
sourceSummary = runtimeDiag->sourceSummary
lastCandidate = runtimeDiag->sourceLastCandidate
sourceOccurrenceEmitted = runtimeDiag->sourceOccurrenceEmitted
acceptedPresent = frequency accepted-present logic
```

ScalarTransient path:

```text
sourceKind = ScalarTransient
sourceSummary = scalar source summary
lastCandidate = scalar last candidate
sourceOccurrenceEmitted = scalar source occurrence emitted
acceptedPresent = scalar accepted-present logic
```

If scalar summary is synthesized, explicitly mark:

```text
sourceSummary.origin = synthesized_scalar_lifecycle
```

Commit:

```text
DetectionCleanup [02-03] Populate shared source stage fields
```

Acceptance:

```text
Both FrequencyMatch and ScalarTransient populate the same outer source fields.
```

---

## 02.4 Move detail fields into namespaced blocks

Action:

Mirror or move fields:

```text
report.frequency.scoreOkFrames
→ report.source.frequencyMatch.scoreOkUpdates

report.frequency.contrastOkFrames
→ report.source.frequencyMatch.contrastOkUpdates

report.frequency.bothOkFrames
→ report.source.frequencyMatch.bothOkUpdates

report.frequency.freqEvidenceClass
→ report.source.frequencyMatch.freqEvidenceClass

report.scalar.scalarRejectReason
→ report.source.scalarTransient.scalarRejectReason

report.scalar.scalarDurationMs
→ report.source.scalarTransient.scalarDurationMs
```

Commit:

```text
DetectionCleanup [02-04] Namespace source-specific report details
```

Acceptance:

```text
Frequency-specific details live under source.frequencyMatch.
Scalar-specific details live under source.scalarTransient.
```

---

## 02.5 Create one common source printer

Target:

```text
AnalyzerReporting.cpp
```

Action:

Create:

```cpp
printSequenceSourceDiagnostics(const SourceStageReport& source);
printFrequencyMatchSourceDetail(const FrequencyMatchSourceDetail& detail);
printScalarTransientSourceDetail(const ScalarTransientSourceDetail& detail);
```

Output shape:

```text
SEQ_SOURCE
SEQ_SOURCE_REJECTS
SEQ_SOURCE_LAST_CANDIDATE
SEQ_SOURCE_DETAIL
```

Commit:

```text
DetectionCleanup [02-05] Add shared source diagnostics printer
```

Acceptance:

```text
SEQ_SOURCE common line is printed by one function for both source kinds.
Source-specific detail is appended by small detail printers only.
```

---

## 02.6 Convert old split printers to wrappers

Target:

```text
printSequenceDiagnostics()
printSequenceScalarDiagnostics()
```

Action:

Either remove them or make them call the common source printer.

Commit:

```text
DetectionCleanup [02-06] Retire split source diagnostics printers
```

Acceptance:

```text
There is no duplicated common source truth printer.
```

---

## 02.7 Summary aggregates stored fields

Action:

SEQ summary must aggregate finalized report fields:

```text
report.source.kind
report.source.frequencyMatch.freqEvidenceClass
report.source.scalarTransient.scalarEvidenceClass
report.classification
```

Do not recalculate source/evidence classes in summary.

Commit:

```text
DetectionCleanup [02-07] Aggregate summary from stored report fields
```

Acceptance:

```text
SEQ_SUMMARY is an aggregation of finalized per-trial report classes only.
```

---

# 03 — Frequency / AMP Frame Parity + FrequencyMatch Cleanup

Goal:

```text
Fresh derived feature values fan out in parallel to FeatureHistory and OccurrenceSource.
FrequencyMatch stays specialized but becomes structurally parallel to ScalarDetector.
Held frequency status is not live detection evidence.
```

## 03.1 Enforce fan-out model in comments/docs first

Add rule near feature producer/runtime code:

```text
Producer emits fresh feature sample/packet.
Producer sends it to FeatureHistory.
Producer sends it to OccurrenceSource.
FeatureHistory is not the live pipe into OccurrenceSource.
```

Commit:

```text
DetectionCleanup [03-01] Document feature fanout rule
```

Acceptance:

```text
Code comments and active docs state parallel fan-out explicitly.
```

---

## 03.2 Formalize AMP feature sample

Action:

Ensure AMP level is represented as a first-class timestamped scalar sample:

```text
AmpEnvelopeSample
```

Flow:

```text
Audio sample packet
→ AmpEnvelopeSample
→ FeatureHistory
→ AmpOccurrenceSource / ScalarOccurrenceSource
```

Commit:

```text
DetectionCleanup [03-02] Formalize AMP envelope sample path
```

Acceptance:

```text
AMP live detection does not pull from FeatureHistory.
AMP feature sample carries timing/index/freshness.
```

---

## 03.3 Formalize frequency scalar samples

Action:

Ensure frequency scalar features exist or are conceptually clear:

```text
FrequencyScoreSample
FrequencyContrastSample
```

Flow:

```text
FrequencyBandMeasurementPacket
→ FrequencyScoreSample
→ FeatureHistory
→ optional ScalarOccurrenceSource

FrequencyBandMeasurementPacket
→ FrequencyContrastSample
→ FeatureHistory
→ optional ScalarOccurrenceSource
```

Commit:

```text
DetectionCleanup [03-03] Formalize frequency scalar samples
```

Acceptance:

```text
FrequencyScore and FrequencyContrast are scalar samples parallel to AmpEnvelope.
```

---

## 03.4 Keep compound frequency packet for FrequencyMatch

Action:

Keep specialized compound packet:

```text
FrequencyBandMeasurementPacket
```

It should contain:

```text
targetHz
targetBandScoreValue
targetBandContrastValue
targetBandPowerValue
neighborBandPowerValue
totalEnergyValue
window timing/indexes
fresh/present
```

It should not contain detector decisions such as:

```text
matched
accepted
valid
reject reason
```

Commit:

```text
DetectionCleanup [03-04] Separate frequency measurement from match decisions
```

Acceptance:

```text
FrequencyMatch consumes compound measurement packet.
Packet is measurement, not evidence/result.
```

---

## 03.5 Fresh-only FrequencyMatch lifecycle

Target:

```text
FrequencyOccurrenceSource
FrequencyMatchDetector
```

Action:

Before attack/release lifecycle update:

```cpp
if (!packet.present || !packet.fresh) {
    observeNoFreshMeasurement(nowMs); // status/debug only, no attack/release
    return;
}
```

At minimum, diagnostic branch should compare old behavior and fresh-only behavior.

Commit:

```text
DetectionCleanup [03-05] Gate FrequencyMatch on fresh measurements
```

Acceptance:

```text
Held/last frequency values do not drive attack/release lifecycle.
fresh_updates and held_status_packets are counted separately.
```

---

## 03.6 Extract FrequencyMatch gate result

Action:

Create or clarify:

```cpp
struct FrequencyMatchGateResult {
    bool scoreAttackOk;
    bool contrastAttackOk;
    bool attackOk;

    bool scoreReleaseOk;
    bool contrastReleaseOk;
    bool releaseOk;

    const char* reason;
};
```

Commit:

```text
DetectionCleanup [03-06] Extract FrequencyMatch gate result
```

Acceptance:

```text
Measurement packet has no match decision.
GateResult owns attack/release threshold decision.
```

---

## 03.7 Align FrequencyMatch lifecycle names with ScalarDetector

Rename lifecycle counters if needed:

```text
currentMatchRunFrames → currentMatchRunUpdates
candidateHoldWindows → candidateHoldUpdates
```

Keep:

```text
candidateHoldMs
candidateFirstSeenMs
candidateLastMatchedMs
refractoryUntilMs
```

Commit:

```text
DetectionCleanup [03-07] Align FrequencyMatch lifecycle counters
```

Acceptance:

```text
Lifecycle counts do not pretend to be windows or frames unless they are.
```

---

## 03.8 Add detector-state visibility

Add trial start/end diagnostics:

```text
candidate_active_at_trial_start
candidate_first_ms
candidate_last_match_ms
candidate_hold_ms
refractory_remaining_ms
opened_this_trial
closed_this_trial
emitted_this_trial
rejected_this_trial
fresh_release_ok_updates
held_release_ok_updates
```

Commit:

```text
DetectionCleanup [03-08] Add detector lifecycle trial diagnostics
```

Acceptance:

```text
Sticky detector lifecycle can be proven or disproven from Analyzer output.
```

---

# 04 — Audio / Detection Naming Cleanup

Goal:

```text
Names reveal layer boundaries.
Value/Sample/Packet/Window/Frame/Evidence/Label are no longer ambiguous.
```

## 04.1 Add active naming glossary

Create/update:

```text
docs/active/DetectionNaming.md
```

Required definitions:

```text
Value       = bare scalar number
Sample      = timestamped scalar observation: value + time/index/freshness
Packet      = live pipeline handoff object; may be compound
Measurement = derived measurement, not evidence yet
Window      = bounded range/slice
Frame       = timing/process/protocol unit only
Block       = source/transport chunk
Evidence    = inspected support/reject payload
Label       = human-readable output only
```

Commit:

```text
DetectionCleanup [04-01] Add detection naming glossary
```

Acceptance:

```text
Active docs contain glossary.
Archive docs untouched.
```

---

## 04.2 Rename AudioSignalFrame to AudioSamplePacket

Target:

```text
AudioSignalFrame type/file/usages
```

Rename:

```text
AudioSignalFrame → AudioSamplePacket
rawSample → rawAudioValue
centeredSample → centeredAudioValue
centeredMagnitude → audioMagnitudeValue
sampleTimeUs → timeUs
sampleTimeMs → timeMs
```

Commit:

```text
DetectionCleanup [04-02] Rename audio signal frame to sample packet
```

Acceptance:

```text
No active code uses AudioSignalFrame.
AudioSamplePacket means one processed raw audio sample plus metadata.
```

---

## 04.3 Rename FrequencyFeatureFrame to FrequencyBandMeasurementPacket

Target:

```text
FrequencyFeatureFrame type/file/usages
```

Rename:

```text
FrequencyFeatureFrame → FrequencyBandMeasurementPacket
evidencePresent → present
updatedThisFrame → fresh
score → targetBandScoreValue
spectralContrast → targetBandContrastValue
targetPower → targetBandPowerValue
neighborPower → neighborBandPowerValue
totalEnergy → totalEnergyValue
windowStartSample → windowStartSampleIndex
windowEndSample → windowEndSampleIndex
windowSampleCount → windowSizeSamples
```

Move out:

```text
matched → FrequencyMatchGateResult
confidence → gate/candidate/evidence if detector-derived
```

Commit:

```text
DetectionCleanup [04-03] Rename frequency feature frame
```

Acceptance:

```text
No active code uses FrequencyFeatureFrame.
Frequency measurement packet does not contain detector decisions.
```

---

## 04.4 Rename frequency cadence fields

Target:

```text
FreqBandStream / FrequencyBandStream
```

Rename:

```text
computeDecimation → frequencyUpdateEverySamples
_computeCountdown → _samplesUntilNextFrequencyUpdate
updatedOnLastObserve() → producedFreshPacketOnLastObserve()
evidenceAgeSamples() → lastPacketAgeSamples()
```

Commit:

```text
DetectionCleanup [04-04] Rename frequency cadence fields
```

Acceptance:

```text
Cadence names describe sample-count update period, not vague decimation.
```

---

## 04.5 Rename target-band fields

Rename:

```text
lastFrequencyScore → lastTargetBandScoreValue
lastSpectralContrast → lastTargetBandContrastValue
lastTargetPower → lastTargetBandPowerValue
lastNeighborPower → lastNeighborBandPowerValue
lastTotalEnergy → lastTotalEnergyValue
frequencyBinSpacingHz → neighborSpacingHz or bandSpacingHz
```

Commit:

```text
DetectionCleanup [04-05] Rename target band value fields
```

Acceptance:

```text
Target-band metrics are not described as generic frequency activity.
```

---

## 04.6 Window audit

Search:

```text
Window
window
```

Keep only true bounded ranges/slices:

```text
ScalarWindow
RawAudioWindow
TrialWindow
GoertzelWindow
InspectorWindow
```

Rename false windows to:

```text
Packet
Measurement
Evidence
Probe
Inspector
```

Commit:

```text
DetectionCleanup [04-06] Audit window naming
```

Acceptance:

```text
Every active “Window” has start/end range semantics.
```

---

## 04.7 Frame audit

Search:

```text
Frame
frame
frames
```

Keep `Frame` only for:

```text
I2S frame
audio processing frame
loop frame
frame duration
processed frame count
```

Rename payload objects to:

```text
Packet
Sample
MeasurementPacket
```

Commit:

```text
DetectionCleanup [04-07] Audit frame naming
```

Acceptance:

```text
Frame no longer means generic feature payload.
```

---

## 04.8 Label audit

Search:

```text
Label
label
```

Keep labels only for human-readable output.

Rename logic-bearing labels to:

```text
Type
Kind
Reason
State
Class
```

Commit:

```text
DetectionCleanup [04-08] Audit label naming
```

Acceptance:

```text
Code logic branches on enums/types/reasons, not labels.
```

---

## 04.9 Update active docs only

Update:

```text
Detection architecture spec
Analyzer roadmap/current diagnostics docs
Detection roadmap/current pass docs
Frequency / AMP Frame Parity item
Miss-streak causes + fixes item
current manuals that mention Analyzer output
```

Do not update:

```text
archive docs
old chat exports
historical snapshots
deprecated docs
```

Commit:

```text
DetectionCleanup [04-09] Update active naming docs
```

Acceptance:

```text
Active docs use current vocabulary.
Archive docs untouched.
```

---

# 05 — Miss-Streak Diagnostics and Fixes

Goal:

```text
Classify bad regimes.
Do not explain long miss streaks as normal acoustics unless proven.
Do not tune blindly.
```

## 05.1 Add compact fault fingerprint line

Add one per trial:

```text
SEQ_STREAK_FAULT trial=N result=miss
audio_present=...
raw_stddev=...
raw_repeat_max=...
target3200_score_max=...
target3200_contrast_max=...
best_band_hz=...
best_band_score=...
best_band_contrast=...
fresh_freq_updates=...
held_freq_frames=...
det_active_start=...
opened_this_trial=...
closed_this_trial=...
emitted_this_trial=...
refractory_remaining_ms=...
timing_lag_max_ms=...
fault_class=...
```

Fault classes:

```text
INPUT_SAMPLE_BAD
OUTPUT_SPECTRAL_SHIFT
OUTPUT_BROADBAND_NON_TONAL
DETECTOR_STUCK_ACTIVE
DETECTOR_REJECTED_TARGET_PRESENT
FRESHNESS_HELD_EVIDENCE_SUSPECT
TIMING_BACKLOG
NO_AUDIO_EVENT
UNKNOWN
```

Commit:

```text
DetectionCleanup [05-01] Add miss fault fingerprint output
```

Acceptance:

```text
Every miss gets one preliminary fault class.
```

---

## 05.2 Add raw sample health stats

Add per trial:

```text
raw_min
raw_max
raw_mean
raw_stddev
raw_mean_abs
dc_offset
zero_cross_rate
same_value_ratio
repeated_sample_max_run
block_hash_repeat_count
raw_health_class
```

Commit:

```text
DetectionCleanup [05-02] Add raw sample health diagnostics
```

Acceptance:

```text
I2S “bytes read” is no longer treated as proof of meaningful audio.
```

---

## 05.3 Add miss-only frequency band scan

Diagnostic-only scan on miss windows:

```text
2800
3000
3200
3400
3600
```

Print:

```text
best_band_hz
best_band_score
best_band_contrast
target3200_score_max
target3200_contrast_max
```

Commit:

```text
DetectionCleanup [05-03] Add miss-only band scan diagnostics
```

Acceptance:

```text
A miss can distinguish no target-band evidence from wrong-band / broadband non-tonal event.
```

---

## 05.4 Add emitter reference markers

Emitter should report:

```text
EMIT_START trial=N t=...
EMIT_DONE trial=N t=...
```

Optional hardware marker:

```text
GPIO high during actual piezo drive
```

Commit:

```text
DetectionCleanup [05-04] Add emitter reference markers
```

Acceptance:

```text
Analyzer can separate command/emitter path failure from input/detector failure.
```

---

## 05.5 Add I2S / mic bad-state checks

Physical/electrical test plan:

```text
logic analyzer on BCLK, WS/LRCLK, DOUT, GND
capture good regime
capture miss streak regime
compare clocks and DOUT activity
```

Build one hardened reference mic node:

```text
short wires
3.3 V verified
100 nF near MEMS VDD/GND
solid ground
I2S wires away from piezo/BTL output
no breadboard if possible
continuous I2S clocking
discard first 500 ms after I2S start
raw sample health log
```

Commit:

```text
DetectionCleanup [05-05] Document I2S bad state checks
```

Acceptance:

```text
One reference node can distinguish shared wiring/layout/power weakness from code-only detector failure.
```

---

## 05.6 Physical isolation matrix

Run short trials:

```text
normal untouched
wiggle emitter/piezo wires only
wiggle MEMS/I2S wires only
touch/move piezo mount only
cover mic
move analyzer only
rotate analyzer only
known-good emitter + current analyzer
current emitter + known-good analyzer
```

Compare only:

```text
fault_class
raw_health
target_band_score
best_band_hz
detector_state
```

Commit:

```text
DetectionCleanup [05-06] Document physical isolation matrix
```

Acceptance:

```text
Natural miss streaks can be matched to an induced fault family.
```

---

# Final Execution Order

## Execute in this order

```text
01 Compatibility sediment fixes
02 Scalar/Frequency reporting parity
03 Frequency/AMP frame parity + FrequencyMatch cleanup
04 Audio/Detection naming cleanup
05 Miss-streak diagnostics and physical/electrical checks
```

Reason:

```text
First remove lying/stale/duplicated truth.
Then make reporting comparable.
Then clean the feature/detector flow.
Then do broad renames.
Then use the improved diagnostics/vocabulary to classify miss streaks.
```

## Final acceptance for whole package

The package is successful when:

```text
Analyzer output no longer has duplicated classification truth.
SEQ_SOURCE has one shared reporting path for Scalar and Frequency.
Frequency and AMP feature flow uses parallel fan-out into History + OccurrenceSource.
FrequencyMatch does not use held frequency status as live evidence.
Naming distinguishes Value, Sample, Packet, Window, Frame, Evidence, Label.
Miss streaks produce fault classes rather than vague “no frequency” conclusions.
```
