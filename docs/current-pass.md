# DET-GENERIC-CANDIDATE-FACTS-v1

Implementation plan for adding generic detector-core candidate facts to the current ResonantNode detection code.

Source snapshot inspected: `src (2).zip`, 2026-06-15.

---

## 1. Goal

Add a small generic fact set for detector candidates / accepted occurrences / selected rejects:

```cpp
float peak = 0.0f;
float mean = 0.0f;       // derived on close/report
float rms = 0.0f;        // derived on close/report

unsigned long coverageAboveAttackMs = 0;
unsigned long coverageAboveReleaseMs = 0;
unsigned long sustainedMs = 0;

unsigned int islandCount = 0;
unsigned int gapCount = 0;
unsigned long islandMaxMs = 0;
unsigned long gapMaxMs = 0;
```

The model is generic scalar strength:

- ScalarTransient uses scalar detector input / signal magnitude as strength.
- FrequencyMatch uses `targetBandScoreValue` as strength.
- Frequency-specific `contrast` stays detector-specific detail.
- Existing `strength` remains for compatibility and should map to `peak` for now.

No new public structs for these facts.

---

## 2. Architectural boundary

### Generic contract lives here

```txt
src/detection/detectors/DetectorReport.h
```

Add fields directly to:

```cpp
AcceptedOccurrenceSummary
SelectedRejectSummary
```

These are the generic report shells currently shared by scalar and frequency.

### Detector-owned runtime state lives here

```txt
src/detection/detectors/scalar/ScalarTransientDetector.h
src/detection/detectors/frequency/FrequencyMatchDetector.h
```

Internal accumulator fields are allowed, but they are not the public contract.

### Do not touch yet

```txt
Occurrence
InspectedOccurrence
PatternResult
Behavior
```

This pass is:

```txt
Detector core -> DetectorReport -> Analyzer / SEQ_SOURCE / SEQ_EXPLAIN
```

not:

```txt
Detector core -> PatternResult -> Behavior
```

---

## 3. Existing naming style

### ScalarTransientDetector

Private fields use `_`:

```cpp
_peakActive
_peakStartedUs
_peakStrength
_transientDurationMs
_acceptedOccurrence
_selectedReject
_reportDetail
```

So new scalar-internal accumulator fields should also use `_candidate...`.

### FrequencyMatchDetector

Current lifecycle fields are mostly public and unprefixed because Analyzer/Runtime still read them directly:

```cpp
pendingActive
pendingPeakScore
pendingDurationMs
bestPeakScore
```

Private fields use `_` only for internal state:

```cpp
_acceptedOccurrence
_pendingOccurrence
_lastEmittedOccurrenceCloseMs
```

For this pass, frequency candidate-fact fields should follow existing pending/best groups:

```cpp
pending...
best...
```

not `_candidate...`.

### DetectorReport

Report fields have no prefix:

```cpp
startMs
peakMs
endMs
durationMs
strength
confidence
```

New generic report fields should also have no prefix:

```cpp
peak
mean
rms
coverageAboveAttackMs
coverageAboveReleaseMs
sustainedMs
islandCount
gapCount
islandMaxMs
gapMaxMs
```

---

## 4. DetectorReport.h changes

Current generic shells:

```cpp
struct AcceptedOccurrenceSummary {
    bool present = false;
    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long endMs = 0;
    unsigned long durationMs = 0;
    float strength = 0.0f;
    float confidence = 0.0f;
};

struct SelectedRejectSummary {
    bool present = false;
    DetectorRejectClass rejectClass = DetectorRejectClass::None;
    const char* detectorReason = "none";
    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long endMs = 0;
    unsigned long durationMs = 0;
    float strength = 0.0f;
    float confidence = 0.0f;
};
```

Add the same generic fact fields to both:

```cpp
float peak = 0.0f;
float mean = 0.0f;
float rms = 0.0f;

unsigned long coverageAboveAttackMs = 0;
unsigned long coverageAboveReleaseMs = 0;
unsigned long sustainedMs = 0;

unsigned int islandCount = 0;
unsigned int gapCount = 0;
unsigned long islandMaxMs = 0;
unsigned long gapMaxMs = 0;
```

Keep:

```cpp
float strength = 0.0f;
```

Compatibility rule:

```cpp
strength == peak
```

for this pass.

---

## 5. ScalarTransientDetector.h changes

Add private runtime accumulator fields near `// Live gate state` or directly after `_peakStrength`.

```cpp
// Generic candidate-fact accumulator state.
float _candidatePeak = 0.0f;
float _candidateSum = 0.0f;
float _candidateSumSquares = 0.0f;
unsigned long _candidateSampleCount = 0;

unsigned long _candidateCoverageAboveAttackMs = 0;
unsigned long _candidateCoverageAboveReleaseMs = 0;
unsigned long _candidateSustainedMs = 0;

unsigned int _candidateIslandCount = 0;
unsigned int _candidateGapCount = 0;
unsigned long _candidateIslandMaxMs = 0;
unsigned long _candidateGapMaxMs = 0;

bool _candidateWasAboveRelease = false;
unsigned long _candidateCurrentIslandStartUs = 0;
unsigned long _candidateCurrentGapStartUs = 0;
unsigned long _candidateLastUpdateUs = 0;
```

Add private helpers:

```cpp
void resetCandidateFacts();
void updateCandidateFacts(
    unsigned long nowUs,
    float strength,
    bool aboveAttackThreshold,
    bool aboveReleaseThreshold
);
void finalizeCandidateFacts(unsigned long releaseObservedUs);
```

No `median` / `p75` in this pass.

---

## 6. ScalarTransientDetector.cpp changes

### 6.1 Reset state

In `resetState()`, after resetting `_peakStrength`, call:

```cpp
resetCandidateFacts();
```

Also call it whenever a candidate closes and `_peakActive` is reset.

### 6.2 Open candidate

In `updateOnsetStage()`, inside:

```cpp
if (aboveAttackThreshold && !_peakActive && onsetCooldownElapsed) {
```

immediately after opening `_peakActive`, call:

```cpp
resetCandidateFacts();
_candidateCurrentIslandStartUs = nowUs;
_candidateWasAboveRelease = true;
_candidateIslandCount = 1;
_candidateLastUpdateUs = nowUs;
```

### 6.3 Update while active

In `update()`, after threshold booleans are known and before close logic, call:

```cpp
if (_peakActive) {
    updateCandidateFacts(nowUs, signalMagnitude, aboveAttackThreshold, aboveReleaseThreshold);
}
```

If `update()` currently only passes `aboveReleaseThreshold` into `updateTransientStage()`, either:

- pass both threshold booleans into `updateTransientStage()`, or
- call `updateCandidateFacts()` from `update()` before `updateTransientStage()`.

Prefer the second option to keep lifecycle logic readable.

### 6.4 Helper behavior

`resetCandidateFacts()`:

```cpp
void ScalarTransientDetector::resetCandidateFacts() {
    _candidatePeak = 0.0f;
    _candidateSum = 0.0f;
    _candidateSumSquares = 0.0f;
    _candidateSampleCount = 0;

    _candidateCoverageAboveAttackMs = 0;
    _candidateCoverageAboveReleaseMs = 0;
    _candidateSustainedMs = 0;

    _candidateIslandCount = 0;
    _candidateGapCount = 0;
    _candidateIslandMaxMs = 0;
    _candidateGapMaxMs = 0;

    _candidateWasAboveRelease = false;
    _candidateCurrentIslandStartUs = 0;
    _candidateCurrentGapStartUs = 0;
    _candidateLastUpdateUs = 0;
}
```

`updateCandidateFacts()` should:

- compute elapsed time since `_candidateLastUpdateUs`
- update peak/sum/sumSquares/sampleCount
- add elapsed ms to coverage fields when thresholds are true
- track islands/gaps based on `aboveReleaseThreshold`
- update max island/gap lengths on transitions

Sketch:

```cpp
void ScalarTransientDetector::updateCandidateFacts(
    unsigned long nowUs,
    float strength,
    bool aboveAttackThreshold,
    bool aboveReleaseThreshold
) {
    const unsigned long deltaUs = _candidateLastUpdateUs == 0 || nowUs < _candidateLastUpdateUs
        ? 0
        : nowUs - _candidateLastUpdateUs;
    const unsigned long deltaMs = deltaUs / 1000UL;

    if (strength > _candidatePeak) {
        _candidatePeak = strength;
    }
    _candidateSum += strength;
    _candidateSumSquares += strength * strength;
    ++_candidateSampleCount;

    if (aboveAttackThreshold) {
        _candidateCoverageAboveAttackMs += deltaMs;
        _candidateSustainedMs += deltaMs;
    }
    if (aboveReleaseThreshold) {
        _candidateCoverageAboveReleaseMs += deltaMs;
    }

    if (aboveReleaseThreshold) {
        if (!_candidateWasAboveRelease) {
            ++_candidateIslandCount;
            if (_candidateCurrentGapStartUs != 0 && nowUs >= _candidateCurrentGapStartUs) {
                const unsigned long gapMs = (nowUs - _candidateCurrentGapStartUs) / 1000UL;
                if (gapMs > _candidateGapMaxMs) {
                    _candidateGapMaxMs = gapMs;
                }
            }
            _candidateCurrentIslandStartUs = nowUs;
            _candidateCurrentGapStartUs = 0;
        }
    } else {
        if (_candidateWasAboveRelease) {
            ++_candidateGapCount;
            if (_candidateCurrentIslandStartUs != 0 && nowUs >= _candidateCurrentIslandStartUs) {
                const unsigned long islandMs = (nowUs - _candidateCurrentIslandStartUs) / 1000UL;
                if (islandMs > _candidateIslandMaxMs) {
                    _candidateIslandMaxMs = islandMs;
                }
            }
            _candidateCurrentGapStartUs = nowUs;
            _candidateCurrentIslandStartUs = 0;
        }
    }

    _candidateWasAboveRelease = aboveReleaseThreshold;
    _candidateLastUpdateUs = nowUs;
}
```

`finalizeCandidateFacts(releaseObservedUs)` should close the currently open island/gap and compute final max lengths.

```cpp
void ScalarTransientDetector::finalizeCandidateFacts(unsigned long releaseObservedUs) {
    if (_candidateWasAboveRelease && _candidateCurrentIslandStartUs != 0 && releaseObservedUs >= _candidateCurrentIslandStartUs) {
        const unsigned long islandMs = (releaseObservedUs - _candidateCurrentIslandStartUs) / 1000UL;
        if (islandMs > _candidateIslandMaxMs) {
            _candidateIslandMaxMs = islandMs;
        }
    }
    if (!_candidateWasAboveRelease && _candidateCurrentGapStartUs != 0 && releaseObservedUs >= _candidateCurrentGapStartUs) {
        const unsigned long gapMs = (releaseObservedUs - _candidateCurrentGapStartUs) / 1000UL;
        if (gapMs > _candidateGapMaxMs) {
            _candidateGapMaxMs = gapMs;
        }
    }
}
```

Mean/rms are derived when copying to report summaries:

```cpp
const float candidateMean = _candidateSampleCount > 0
    ? _candidateSum / static_cast<float>(_candidateSampleCount)
    : 0.0f;
const float candidateRms = _candidateSampleCount > 0
    ? sqrtf(_candidateSumSquares / static_cast<float>(_candidateSampleCount))
    : 0.0f;
```

Add:

```cpp
#include <math.h>
```

if not already available.

### 6.5 Copy to accepted summary

In `captureAcceptedOccurrence()`:

```cpp
finalizeCandidateFacts(releaseObservedUs);

const float candidateMean = _candidateSampleCount > 0
    ? _candidateSum / static_cast<float>(_candidateSampleCount)
    : 0.0f;
const float candidateRms = _candidateSampleCount > 0
    ? sqrtf(_candidateSumSquares / static_cast<float>(_candidateSampleCount))
    : 0.0f;

_acceptedOccurrence.peak = _candidatePeak;
_acceptedOccurrence.mean = candidateMean;
_acceptedOccurrence.rms = candidateRms;
_acceptedOccurrence.coverageAboveAttackMs = _candidateCoverageAboveAttackMs;
_acceptedOccurrence.coverageAboveReleaseMs = _candidateCoverageAboveReleaseMs;
_acceptedOccurrence.sustainedMs = _candidateSustainedMs;
_acceptedOccurrence.islandCount = _candidateIslandCount;
_acceptedOccurrence.gapCount = _candidateGapCount;
_acceptedOccurrence.islandMaxMs = _candidateIslandMaxMs;
_acceptedOccurrence.gapMaxMs = _candidateGapMaxMs;

_acceptedOccurrence.strength = _acceptedOccurrence.peak;
```

Current code uses:

```cpp
_acceptedOccurrence.strength = _peakStrength;
```

Replace or keep consistent:

```cpp
_acceptedOccurrence.peak = _peakStrength; // or _candidatePeak if already updated reliably
_acceptedOccurrence.strength = _acceptedOccurrence.peak;
```

Preferred: `_candidatePeak`, with `_peakStrength` kept as lifecycle peak until later cleanup.

### 6.6 Copy to selected reject

In `captureSelectedReject()`:

```cpp
finalizeCandidateFacts(releaseObservedUs);

_selectedReject.peak = _candidatePeak;
_selectedReject.mean = candidateMean;
_selectedReject.rms = candidateRms;
_selectedReject.coverageAboveAttackMs = _candidateCoverageAboveAttackMs;
_selectedReject.coverageAboveReleaseMs = _candidateCoverageAboveReleaseMs;
_selectedReject.sustainedMs = _candidateSustainedMs;
_selectedReject.islandCount = _candidateIslandCount;
_selectedReject.gapCount = _candidateGapCount;
_selectedReject.islandMaxMs = _candidateIslandMaxMs;
_selectedReject.gapMaxMs = _candidateGapMaxMs;

_selectedReject.strength = _selectedReject.peak;
```

But note: current selected-reject comparison uses `_lastTransientRejectedStrength`. For this pass keep the comparator unchanged, but set report facts after a reject wins.

---

## 7. FrequencyMatchDetector.h changes

Add pending accumulator state near existing `pendingPeakScore` / `pendingPeakContrast`:

```cpp
float pendingSum = 0.0f;
float pendingSumSquares = 0.0f;
unsigned long pendingSampleCount = 0;

unsigned long pendingCoverageAboveAttackMs = 0;
unsigned long pendingCoverageAboveReleaseMs = 0;
unsigned long pendingSustainedMs = 0;

unsigned int pendingIslandCount = 0;
unsigned int pendingGapCount = 0;
unsigned long pendingIslandMaxMs = 0;
unsigned long pendingGapMaxMs = 0;

bool pendingWasAboveRelease = false;
unsigned long pendingCurrentIslandStartMs = 0;
unsigned long pendingCurrentGapStartMs = 0;
unsigned long pendingLastUpdateMs = 0;
```

Add best-reject copies near `bestPeakScore`:

```cpp
float bestMean = 0.0f;
float bestRms = 0.0f;
unsigned long bestCoverageAboveAttackMs = 0;
unsigned long bestCoverageAboveReleaseMs = 0;
unsigned long bestSustainedMs = 0;
unsigned int bestIslandCount = 0;
unsigned int bestGapCount = 0;
unsigned long bestIslandMaxMs = 0;
unsigned long bestGapMaxMs = 0;
```

Add private helpers:

```cpp
void resetPendingFacts();
void updatePendingFacts(
    unsigned long nowMs,
    float strength,
    bool aboveAttackThreshold,
    bool aboveReleaseThreshold
);
void finalizePendingFacts(unsigned long closeMs);
float pendingMean() const;
float pendingRms() const;
```

---

## 8. FrequencyMatchDetector.cpp changes

### 8.1 Strength source

Use frequency score as generic strength:

```cpp
const float strength = evidence.targetBandScoreValue;
```

Do not use contrast for generic facts. Contrast remains here:

```cpp
out.frequency.accepted.contrast
out.frequency.selectedReject.contrast
```

### 8.2 Reset

In `resetState()` and wherever pending is cleared:

```cpp
resetPendingFacts();
```

Also reset best fields in `resetRejectSummary()`.

### 8.3 Open pending

When pending opens, currently `pendingPeakScore`, `pendingPeakContrast`, `pendingOpenMs`, etc. are initialized. Add:

```cpp
resetPendingFacts();
pendingPeakScore = evidence.targetBandScoreValue;
pendingWasAboveRelease = true;
pendingIslandCount = 1;
pendingCurrentIslandStartMs = now;
pendingLastUpdateMs = now;
```

### 8.4 Update pending

For each fresh evidence frame while pending is active:

```cpp
updatePendingFacts(
    now,
    evidence.targetBandScoreValue,
    attackScoreOk,
    releaseScoreOk
);
```

Question: should generic release use only score or score+contrast?

Decision for v1:

```txt
coverageAboveAttackMs / coverageAboveReleaseMs follow score thresholds only.
```

Reason: generic strength is score. Contrast remains detector-specific support/gate evidence.

Alternative later: add frequency-specific detail for contrast coverage if useful.

### 8.5 Finalize on close

Before setting accepted/rejected report state:

```cpp
finalizePendingFacts(pendingCloseMs);
```

Derived values:

```cpp
float FrequencyMatchDetector::pendingMean() const {
    return pendingSampleCount > 0
        ? pendingSum / static_cast<float>(pendingSampleCount)
        : 0.0f;
}

float FrequencyMatchDetector::pendingRms() const {
    return pendingSampleCount > 0
        ? sqrtf(pendingSumSquares / static_cast<float>(pendingSampleCount))
        : 0.0f;
}
```

Add:

```cpp
#include <math.h>
```

if needed.

### 8.6 Accepted copy

When `_acceptedOccurrence` is filled from pending occurrence, add:

```cpp
_acceptedOccurrence.peak = pendingPeakScore;
_acceptedOccurrence.mean = pendingMean();
_acceptedOccurrence.rms = pendingRms();
_acceptedOccurrence.coverageAboveAttackMs = pendingCoverageAboveAttackMs;
_acceptedOccurrence.coverageAboveReleaseMs = pendingCoverageAboveReleaseMs;
_acceptedOccurrence.sustainedMs = pendingSustainedMs;
_acceptedOccurrence.islandCount = pendingIslandCount;
_acceptedOccurrence.gapCount = pendingGapCount;
_acceptedOccurrence.islandMaxMs = pendingIslandMaxMs;
_acceptedOccurrence.gapMaxMs = pendingGapMaxMs;

_acceptedOccurrence.strength = _acceptedOccurrence.peak;
```

### 8.7 Selected reject copy

When recording best rejected pending, also copy:

```cpp
bestMean = pendingMean();
bestRms = pendingRms();
bestCoverageAboveAttackMs = pendingCoverageAboveAttackMs;
bestCoverageAboveReleaseMs = pendingCoverageAboveReleaseMs;
bestSustainedMs = pendingSustainedMs;
bestIslandCount = pendingIslandCount;
bestGapCount = pendingGapCount;
bestIslandMaxMs = pendingIslandMaxMs;
bestGapMaxMs = pendingGapMaxMs;
```

Then in `buildReport()` selected reject block:

```cpp
out.selectedReject.peak = bestPeakScore;
out.selectedReject.mean = bestMean;
out.selectedReject.rms = bestRms;
out.selectedReject.coverageAboveAttackMs = bestCoverageAboveAttackMs;
out.selectedReject.coverageAboveReleaseMs = bestCoverageAboveReleaseMs;
out.selectedReject.sustainedMs = bestSustainedMs;
out.selectedReject.islandCount = bestIslandCount;
out.selectedReject.gapCount = bestGapCount;
out.selectedReject.islandMaxMs = bestIslandMaxMs;
out.selectedReject.gapMaxMs = bestGapMaxMs;
out.selectedReject.strength = out.selectedReject.peak;
```

---

## 9. DetectorReportPrinter.cpp changes

Current generic printer emits:

```txt
accepted.duration_ms=
accepted.strength=
reject.duration_ms=
reject.strength=
```

Add generic facts after strength/confidence for accepted and reject.

Accepted:

```cpp
Serial.print(" accepted.peak=");
Serial.print(accepted.peak, 1);
Serial.print(" accepted.mean=");
Serial.print(accepted.mean, 1);
Serial.print(" accepted.rms=");
Serial.print(accepted.rms, 1);
Serial.print(" accepted.coverage_attack_ms=");
Serial.print(accepted.coverageAboveAttackMs);
Serial.print(" accepted.coverage_release_ms=");
Serial.print(accepted.coverageAboveReleaseMs);
Serial.print(" accepted.sustained_ms=");
Serial.print(accepted.sustainedMs);
Serial.print(" accepted.island_count=");
Serial.print(accepted.islandCount);
Serial.print(" accepted.gap_count=");
Serial.print(accepted.gapCount);
Serial.print(" accepted.island_max_ms=");
Serial.print(accepted.islandMaxMs);
Serial.print(" accepted.gap_max_ms=");
Serial.print(accepted.gapMaxMs);
```

Reject:

```cpp
Serial.print(" reject.peak=");
Serial.print(selectedReject.peak, 1);
Serial.print(" reject.mean=");
Serial.print(selectedReject.mean, 1);
Serial.print(" reject.rms=");
Serial.print(selectedReject.rms, 1);
Serial.print(" reject.coverage_attack_ms=");
Serial.print(selectedReject.coverageAboveAttackMs);
Serial.print(" reject.coverage_release_ms=");
Serial.print(selectedReject.coverageAboveReleaseMs);
Serial.print(" reject.sustained_ms=");
Serial.print(selectedReject.sustainedMs);
Serial.print(" reject.island_count=");
Serial.print(selectedReject.islandCount);
Serial.print(" reject.gap_count=");
Serial.print(selectedReject.gapCount);
Serial.print(" reject.island_max_ms=");
Serial.print(selectedReject.islandMaxMs);
Serial.print(" reject.gap_max_ms=");
Serial.print(selectedReject.gapMaxMs);
```

Keep `accepted.strength` and `reject.strength` printed for compatibility.

---

## 10. Do not add p75 / median now

Deferred:

```cpp
p75
median
islandMinMs
islandMeanMs
gapMinMs
gapMeanMs
```

Reason:

- `mean` and `rms` are cheap derived values from sum/sumSquares/sampleCount.
- `p75` / `median` require a bounded quantile tracker, histogram, or sample buffer.
- min/mean island/gap needs more aggregate state and is not needed for v1.

---

## 11. Acceptance policy stays unchanged

Do not use these new facts as gates yet.

Current acceptance remains:

Scalar:

```txt
durationAccepted && strengthAccepted
```

Frequency:

```txt
score / contrast / duration / release / gate rules as currently implemented
```

New fields are diagnostics first.

Later possible use:

```txt
fragmentation-heavy reject
rms/p75-based quality
coverage-based validity
```

not in this pass.

---

## 12. Suggested implementation passes

### Pass A — Generic report fields

Files:

```txt
DetectorReport.h
DetectorReportPrinter.cpp
```

Tasks:

- Add fields to `AcceptedOccurrenceSummary` and `SelectedRejectSummary`.
- Print fields in generic line.
- Compile.
- Confirm zero/default output when detectors do not populate yet.

### Pass B — Scalar accumulator

Files:

```txt
ScalarTransientDetector.h
ScalarTransientDetector.cpp
```

Tasks:

- Add `_candidate...` accumulator fields.
- Add reset/update/finalize helpers.
- Populate accepted/reject report fields.
- Keep `strength == peak`.
- Compile.
- Run existing scalar/TonalPulse sequence.

### Pass C — Frequency accumulator

Files:

```txt
FrequencyMatchDetector.h
FrequencyMatchDetector.cpp
```

Tasks:

- Add `pending...` and `best...` fields.
- Use `targetBandScoreValue` as generic strength.
- Populate accepted/reject report fields.
- Keep score/contrast specific details unchanged.
- Compile.
- Run FrequencyMatch / TonalPulse sequence.

### Pass D — Analyzer output check

Files likely touched only if print parsing/tests expect exact keys:

```txt
AnalyzerSeqReporter.cpp
AnalyzerSequenceSession.cpp
AnalyzerCommands.cpp
```

Tasks:

- Verify `SEQ_SOURCE`, `SEQ_EXPLAIN`, `SEQ_SUMMARY` tolerate added key-value fields.
- Update any snapshot tests / ps1 parsing if strict.

---

## 13. Expected output shape

Example generic line after pass:

```txt
SEQ_SOURCE trial=1 detector=ScalarTransient ...
accepted.present=1
accepted.duration_ms=42
accepted.strength=123.4
accepted.peak=123.4
accepted.mean=87.2
accepted.rms=91.5
accepted.coverage_attack_ms=28
accepted.coverage_release_ms=37
accepted.sustained_ms=28
accepted.island_count=1
accepted.gap_count=0
accepted.island_max_ms=37
accepted.gap_max_ms=0
...
```

Frequency example:

```txt
SEQ_SOURCE trial=1 detector=FrequencyMatch ...
accepted.strength=0.84
accepted.peak=0.84
accepted.mean=0.61
accepted.rms=0.66
...
detail.frequency.accepted.score=0.84
detail.frequency.accepted.contrast=...
```

---

## 14. Review checklist

Compile checks:

```txt
No missing math include
No struct field name collision
No strict printer/test parsing breakage
No behavior change in accepted/rejected counts
No PatternResult shape change
```

Runtime checks:

```txt
accepted.strength == accepted.peak
reject.strength == reject.peak
mean <= peak for normal positive strength values
rms <= peak for normal positive strength values
coverageAboveAttackMs <= durationMs approximately
coverageAboveReleaseMs <= durationMs approximately
islandCount >= 1 for normal accepted events
gapCount == 0 for clean unfragmented events
gapCount > 0 for debounce/fragmented events
```

Known approximation:

```txt
coverage fields are based on loop/evidence update cadence, not exact sample-time integration.
```

---

## 15. Final target wording

Add flat generic detector candidate facts to the existing DetectorReport accepted/reject summaries. Scalar and Frequency detectors both treat their detector input as a scalar strength value: scalar signal magnitude for ScalarTransient, frequency score for FrequencyMatch. Detectors keep minimal internal accumulator state to derive mean/rms and fragmentation facts. The new facts are diagnostic/report facts only and do not change acceptance policy, Occurrence, PatternResult, or Behavior.
