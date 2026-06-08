# Band Evidence Diagnostic — Implementation Plan

Context: current code should expose the three Goertzel band values directly:

- target band
- lower neighbor band
- upper neighbor band

Instead of collapsing the two neighbor bands only into a contrast value, the diagnostic path should carry and aggregate the per-band values so Analyzer can show whether a miss was caused by:

- no target-band energy
- target energy but weak dominance
- energy in the wrong neighbor band
- source/detector timing or duration gates
- pattern rejection unrelated to frequency strength

This is a **current-code diagnostic pass**, not a new architecture layer.

---

## Goal

Current path:

```txt
FreqBandStream
→ FrequencyBandMeasurementPacket
→ FrequencyMatchDetector
→ DetectionDiagnostics
→ AnalyzerFrequencyDiagnostic
→ SEQ_SOURCE_DIAG
```

Add band evidence to this same path.

The Goertzel frame / measurement packet should contain:

```txt
target band power / score
lower band power / score
upper band power / score
neighbor mean power
neighbor max power
contrast
```

The detector should aggregate over the trial/diagnostic window:

```txt
max per band
mean per band
timestamp of max per band
```

Analyzer should print those values.

---

## Non-goal for this pass

Do **not** add new `FeatureHistory` streams yet.

Avoid this for now:

```cpp
FrequencyTargetPower
FrequencyLowerPower
FrequencyUpperPower
```

Reason: this increases stream count / RAM pressure. For trial-window max/mean diagnostics, the live packet plus `FrequencyMatchDetector` diagnostics are enough.

Candidate-relative `TargetBandStrengthInspector` can come later if needed.

---

## 1. Extend `FreqBandStream`

Done.

File:

```txt
src/detection/features/FreqBandStream.h
```

Add getters:

```cpp
float lastLowerBandPowerValue() const;
float lastUpperBandPowerValue() const;
float lastLowerBandScoreValue() const;
float lastUpperBandScoreValue() const;
float lastNeighborBandPowerMaxValue() const;
```

Add private fields:

```cpp
float _lastLowerBandPowerValue = 0.0f;
float _lastUpperBandPowerValue = 0.0f;
float _lastLowerBandScoreValue = 0.0f;
float _lastUpperBandScoreValue = 0.0f;
float _lastNeighborBandPowerMaxValue = 0.0f;
```

Reset these fields in:

```cpp
resetState()
```

and in any “window not ready” / invalid state branch.

---

## 2. Update `FreqBandStream::computeFrequencyScore()`

Done.

File:

```txt
src/detection/features/FreqBandStream.cpp
```

Current shape is probably similar to:

```cpp
const float targetPower = computeGoertzelPowerAtFrequency(_cachedTargetFrequencyHz);
const float lowerPower = computeGoertzelPowerAtFrequency(_cachedLowerFrequencyHz);
const float upperPower = computeGoertzelPowerAtFrequency(_cachedUpperFrequencyHz);

const float neighborPower = (lowerPower + upperPower) * 0.5f;
const float normalized = (targetPower * 1000.0f) / (totalEnergy + 1.0f);
const float contrast = targetPower / (neighborPower + 1.0f);
```

Change/extend to:

```cpp
const float targetPower = computeGoertzelPowerAtFrequency(_cachedTargetFrequencyHz);
const float lowerPower = computeGoertzelPowerAtFrequency(_cachedLowerFrequencyHz);
const float upperPower = computeGoertzelPowerAtFrequency(_cachedUpperFrequencyHz);

const float neighborPowerMean = (lowerPower + upperPower) * 0.5f;
const float neighborPowerMax = lowerPower > upperPower ? lowerPower : upperPower;

const float targetScore = (targetPower * 1000.0f) / (totalEnergy + 1.0f);
const float lowerScore = (lowerPower * 1000.0f) / (totalEnergy + 1.0f);
const float upperScore = (upperPower * 1000.0f) / (totalEnergy + 1.0f);

const float contrast = targetPower / (neighborPowerMean + 1.0f);
```

Then assign:

```cpp
_lastTargetBandScoreValue = targetScore;
_lastTargetBandPowerValue = targetPower;

_lastLowerBandPowerValue = lowerPower;
_lastUpperBandPowerValue = upperPower;
_lastLowerBandScoreValue = lowerScore;
_lastUpperBandScoreValue = upperScore;

_lastNeighborBandPowerValue = neighborPowerMean;        // keep old field meaning
_lastNeighborBandPowerMaxValue = neighborPowerMax;

_lastTotalEnergyValue = totalEnergy;
_lastTargetBandContrastValue = contrast;
```

Add getter implementations:

```cpp
float FreqBandStream::lastLowerBandPowerValue() const {
    return _lastLowerBandPowerValue;
}

float FreqBandStream::lastUpperBandPowerValue() const {
    return _lastUpperBandPowerValue;
}

float FreqBandStream::lastLowerBandScoreValue() const {
    return _lastLowerBandScoreValue;
}

float FreqBandStream::lastUpperBandScoreValue() const {
    return _lastUpperBandScoreValue;
}

float FreqBandStream::lastNeighborBandPowerMaxValue() const {
    return _lastNeighborBandPowerMaxValue;
}
```

---

## 3. Extend `FrequencyBandMeasurementPacket`

Done.

File:

```txt
src/detection/inspector/InspectorTypes.h
```

Current struct probably contains:

```cpp
struct FrequencyBandMeasurementPacket {
    bool present = false;
    bool matched = false;
    bool fresh = false;

    unsigned long targetHz = 0;
    unsigned long observedAtMs = 0;
    unsigned long ageSamples = 0;

    float targetBandScoreValue = 0.0f;
    float confidence = 0.0f;

    float targetBandPowerValue = 0.0f;
    float neighborBandPowerValue = 0.0f;
    float totalEnergyValue = 0.0f;
    float targetBandContrastValue = 0.0f;
};
```

Extend it like this:

```cpp
struct FrequencyBandMeasurementPacket {
    bool present = false;
    bool matched = false;
    bool fresh = false;

    unsigned long targetHz = 0;
    unsigned long observedAtMs = 0;
    unsigned long ageSamples = 0;

    float targetBandScoreValue = 0.0f;
    float confidence = 0.0f;

    float targetBandPowerValue = 0.0f;

    // Existing meaning: mean of lower and upper neighbor powers.
    float neighborBandPowerValue = 0.0f;

    // New explicit band evidence.
    float lowerBandPowerValue = 0.0f;
    float upperBandPowerValue = 0.0f;
    float lowerBandScoreValue = 0.0f;
    float upperBandScoreValue = 0.0f;
    float neighborBandPowerMaxValue = 0.0f;

    float totalEnergyValue = 0.0f;
    float targetBandContrastValue = 0.0f;
};
```

Compatibility rule:

- keep `neighborBandPowerValue`
- keep its current meaning as **mean neighbor power**
- add `neighborBandPowerMaxValue` separately

---

## 4. Fill the packet builder

Done.

File:

```txt
src/detection/features/FrequencyMeasurementPacketBuilder.h
```

Current code likely copies:

```cpp
evidence.targetBandPowerValue = freqBandStream.lastTargetBandPowerValue();
evidence.neighborBandPowerValue = freqBandStream.lastNeighborBandPowerValue();
evidence.totalEnergyValue = freqBandStream.lastTotalEnergyValue();
evidence.targetBandContrastValue = freqBandStream.lastTargetBandContrastValue();
```

Extend to:

```cpp
evidence.targetBandPowerValue = freqBandStream.lastTargetBandPowerValue();
evidence.neighborBandPowerValue = freqBandStream.lastNeighborBandPowerValue();

evidence.lowerBandPowerValue = freqBandStream.lastLowerBandPowerValue();
evidence.upperBandPowerValue = freqBandStream.lastUpperBandPowerValue();
evidence.lowerBandScoreValue = freqBandStream.lastLowerBandScoreValue();
evidence.upperBandScoreValue = freqBandStream.lastUpperBandScoreValue();
evidence.neighborBandPowerMaxValue = freqBandStream.lastNeighborBandPowerMaxValue();

evidence.totalEnergyValue = freqBandStream.lastTotalEnergyValue();
evidence.targetBandContrastValue = freqBandStream.lastTargetBandContrastValue();
```

After this step, each fresh Goertzel measurement packet carries real band facts.

---

## 5. Add compact band stats to `FrequencyMatchDetector`

Done.

File:

```txt
src/detection/detectors/FrequencyMatchDetector.h
```

Add a helper struct:

```cpp
struct FrequencyBandDiagnosticStats {
    float sum = 0.0f;
    float min = 0.0f;
    float max = 0.0f;
    unsigned long maxMs = 0;
};
```

Inside `FrequencyMatchDetector`, add:

```cpp
FrequencyBandDiagnosticStats diagnosticsTargetPower = {};
FrequencyBandDiagnosticStats diagnosticsLowerPower = {};
FrequencyBandDiagnosticStats diagnosticsUpperPower = {};
FrequencyBandDiagnosticStats diagnosticsNeighborPowerMean = {};
FrequencyBandDiagnosticStats diagnosticsNeighborPowerMax = {};
FrequencyBandDiagnosticStats diagnosticsLowerScore = {};
FrequencyBandDiagnosticStats diagnosticsUpperScore = {};

bool _diagnosticsHaveBandStats = false;
```

Add mean helpers:

```cpp
float diagnosticsTargetPowerMean() const;
float diagnosticsLowerPowerMean() const;
float diagnosticsUpperPowerMean() const;
float diagnosticsNeighborPowerMeanValue() const;
float diagnosticsNeighborPowerMaxMean() const;
float diagnosticsLowerScoreMean() const;
float diagnosticsUpperScoreMean() const;
```

Reset all stats in:

```cpp
resetDiagnosticsSummary()
```

---

## 6. Update stats in `FrequencyMatchDetector::update()`

Done.

File:

```txt
src/detection/detectors/FrequencyMatchDetector.cpp
```

Inside the existing diagnostics block:

```cpp
if (evidence.present) {
    ...
}
```

add:

```cpp
const bool firstBandStats = !_diagnosticsHaveBandStats;

const auto updateBandStats = [&](FrequencyBandDiagnosticStats& stats, float value) {
    stats.sum += value;

    if (firstBandStats) {
        stats.min = value;
        stats.max = value;
        stats.maxMs = now;
        return;
    }

    if (value < stats.min) {
        stats.min = value;
    }

    if (value > stats.max) {
        stats.max = value;
        stats.maxMs = now;
    }
};

updateBandStats(diagnosticsTargetPower, evidence.targetBandPowerValue);
updateBandStats(diagnosticsLowerPower, evidence.lowerBandPowerValue);
updateBandStats(diagnosticsUpperPower, evidence.upperBandPowerValue);
updateBandStats(diagnosticsNeighborPowerMean, evidence.neighborBandPowerValue);
updateBandStats(diagnosticsNeighborPowerMax, evidence.neighborBandPowerMaxValue);
updateBandStats(diagnosticsLowerScore, evidence.lowerBandScoreValue);
updateBandStats(diagnosticsUpperScore, evidence.upperBandScoreValue);

_diagnosticsHaveBandStats = true;
```

Mean helper example:

```cpp
float FrequencyMatchDetector::diagnosticsTargetPowerMean() const {
    return diagnosticsObservedCount > 0
        ? diagnosticsTargetPower.sum / static_cast<float>(diagnosticsObservedCount)
        : 0.0f;
}
```

Use the same pattern for:

```cpp
diagnosticsLowerPowerMean()
diagnosticsUpperPowerMean()
diagnosticsNeighborPowerMeanValue()
diagnosticsNeighborPowerMaxMean()
diagnosticsLowerScoreMean()
diagnosticsUpperScoreMean()
```

Important:

```txt
Only aggregate evidence.present frames.
Do not aggregate held / non-present / invalid frames.
```

---

## 7. Extend `DetectionDiagnostics`

Done.

File:

```txt
src/detection/DetectionRuntime.h
```

Add fields:

```cpp
float frequencyTargetPowerMean = 0.0f;
float frequencyLowerPowerMean = 0.0f;
float frequencyUpperPowerMean = 0.0f;
float frequencyNeighborPowerMean = 0.0f;
float frequencyNeighborPowerMaxMean = 0.0f;

float frequencyTargetPowerMax = 0.0f;
float frequencyLowerPowerMax = 0.0f;
float frequencyUpperPowerMax = 0.0f;
float frequencyNeighborPowerMeanMax = 0.0f;
float frequencyNeighborPowerMaxMax = 0.0f;

unsigned long frequencyTargetPowerMaxMs = 0;
unsigned long frequencyLowerPowerMaxMs = 0;
unsigned long frequencyUpperPowerMaxMs = 0;
unsigned long frequencyNeighborPowerMeanMaxMs = 0;
unsigned long frequencyNeighborPowerMaxMaxMs = 0;

float frequencyLowerScoreMean = 0.0f;
float frequencyUpperScoreMean = 0.0f;
float frequencyLowerScoreMax = 0.0f;
float frequencyUpperScoreMax = 0.0f;

unsigned long frequencyLowerScoreMaxMs = 0;
unsigned long frequencyUpperScoreMaxMs = 0;
```

---

## 8. Fill `DetectionDiagnostics`

Done.

File:

```txt
src/detection/DetectionRuntime.cpp
```

In the method that captures diagnostics from the detector, add:

```cpp
_diagnostics.frequencyTargetPowerMean = detector.diagnosticsTargetPowerMean();
_diagnostics.frequencyLowerPowerMean = detector.diagnosticsLowerPowerMean();
_diagnostics.frequencyUpperPowerMean = detector.diagnosticsUpperPowerMean();
_diagnostics.frequencyNeighborPowerMean = detector.diagnosticsNeighborPowerMeanValue();
_diagnostics.frequencyNeighborPowerMaxMean = detector.diagnosticsNeighborPowerMaxMean();

_diagnostics.frequencyTargetPowerMax = detector.diagnosticsTargetPower.max;
_diagnostics.frequencyLowerPowerMax = detector.diagnosticsLowerPower.max;
_diagnostics.frequencyUpperPowerMax = detector.diagnosticsUpperPower.max;
_diagnostics.frequencyNeighborPowerMeanMax = detector.diagnosticsNeighborPowerMean.max;
_diagnostics.frequencyNeighborPowerMaxMax = detector.diagnosticsNeighborPowerMax.max;

_diagnostics.frequencyTargetPowerMaxMs = detector.diagnosticsTargetPower.maxMs;
_diagnostics.frequencyLowerPowerMaxMs = detector.diagnosticsLowerPower.maxMs;
_diagnostics.frequencyUpperPowerMaxMs = detector.diagnosticsUpperPower.maxMs;
_diagnostics.frequencyNeighborPowerMeanMaxMs = detector.diagnosticsNeighborPowerMean.maxMs;
_diagnostics.frequencyNeighborPowerMaxMaxMs = detector.diagnosticsNeighborPowerMax.maxMs;

_diagnostics.frequencyLowerScoreMean = detector.diagnosticsLowerScoreMean();
_diagnostics.frequencyUpperScoreMean = detector.diagnosticsUpperScoreMean();
_diagnostics.frequencyLowerScoreMax = detector.diagnosticsLowerScore.max;
_diagnostics.frequencyUpperScoreMax = detector.diagnosticsUpperScore.max;
_diagnostics.frequencyLowerScoreMaxMs = detector.diagnosticsLowerScore.maxMs;
_diagnostics.frequencyUpperScoreMaxMs = detector.diagnosticsUpperScore.maxMs;
```

---

## 9. Extend `AnalyzerFrequencyDiagnostic`

Done.

File:

```txt
src/modes/analyzer/AnalyzerReporting.h
```

Add fields near existing frequency score / contrast fields:

```cpp
float targetPowerMean = 0.0f;
float lowerPowerMean = 0.0f;
float upperPowerMean = 0.0f;
float neighborPowerMean = 0.0f;
float neighborPowerMaxMean = 0.0f;

float targetPowerMax = 0.0f;
float lowerPowerMax = 0.0f;
float upperPowerMax = 0.0f;
float neighborPowerMeanMax = 0.0f;
float neighborPowerMaxMax = 0.0f;

unsigned long targetPowerMaxMs = 0;
unsigned long lowerPowerMaxMs = 0;
unsigned long upperPowerMaxMs = 0;
unsigned long neighborPowerMeanMaxMs = 0;
unsigned long neighborPowerMaxMaxMs = 0;

float lowerScoreMean = 0.0f;
float upperScoreMean = 0.0f;
float lowerScoreMax = 0.0f;
float upperScoreMax = 0.0f;

unsigned long lowerScoreMaxMs = 0;
unsigned long upperScoreMaxMs = 0;
```

---

## 10. Copy diagnostics into Analyzer report

Done.

File:

```txt
src/modes/analyzer/AnalyzerApp.cpp
```

Where `report.source.frequencyMatch` is filled from `runtimeDiag`, add:

```cpp
report.source.frequencyMatch.targetPowerMean = runtimeDiag->frequencyTargetPowerMean;
report.source.frequencyMatch.lowerPowerMean = runtimeDiag->frequencyLowerPowerMean;
report.source.frequencyMatch.upperPowerMean = runtimeDiag->frequencyUpperPowerMean;
report.source.frequencyMatch.neighborPowerMean = runtimeDiag->frequencyNeighborPowerMean;
report.source.frequencyMatch.neighborPowerMaxMean = runtimeDiag->frequencyNeighborPowerMaxMean;

report.source.frequencyMatch.targetPowerMax = runtimeDiag->frequencyTargetPowerMax;
report.source.frequencyMatch.lowerPowerMax = runtimeDiag->frequencyLowerPowerMax;
report.source.frequencyMatch.upperPowerMax = runtimeDiag->frequencyUpperPowerMax;
report.source.frequencyMatch.neighborPowerMeanMax = runtimeDiag->frequencyNeighborPowerMeanMax;
report.source.frequencyMatch.neighborPowerMaxMax = runtimeDiag->frequencyNeighborPowerMaxMax;

report.source.frequencyMatch.targetPowerMaxMs = runtimeDiag->frequencyTargetPowerMaxMs;
report.source.frequencyMatch.lowerPowerMaxMs = runtimeDiag->frequencyLowerPowerMaxMs;
report.source.frequencyMatch.upperPowerMaxMs = runtimeDiag->frequencyUpperPowerMaxMs;
report.source.frequencyMatch.neighborPowerMeanMaxMs = runtimeDiag->frequencyNeighborPowerMeanMaxMs;
report.source.frequencyMatch.neighborPowerMaxMaxMs = runtimeDiag->frequencyNeighborPowerMaxMaxMs;

report.source.frequencyMatch.lowerScoreMean = runtimeDiag->frequencyLowerScoreMean;
report.source.frequencyMatch.upperScoreMean = runtimeDiag->frequencyUpperScoreMean;
report.source.frequencyMatch.lowerScoreMax = runtimeDiag->frequencyLowerScoreMax;
report.source.frequencyMatch.upperScoreMax = runtimeDiag->frequencyUpperScoreMax;
report.source.frequencyMatch.lowerScoreMaxMs = runtimeDiag->frequencyLowerScoreMaxMs;
report.source.frequencyMatch.upperScoreMaxMs = runtimeDiag->frequencyUpperScoreMaxMs;
```

---

## 11. Print in `SEQ_SOURCE_DIAG`

Done.

File:

```txt
src/modes/analyzer/AnalyzerReporting.cpp
```

In `printFrequencyMatchSourceDetail()`, add compact fields:

```cpp
Serial.print(' ');
printField(AnalyzerFieldDescriptor{"source.freq.band", "target_power_max"}, frequencySource.targetPowerMax, 1);
Serial.print(' ');
printField(AnalyzerFieldDescriptor{"source.freq.band", "lower_power_max"}, frequencySource.lowerPowerMax, 1);
Serial.print(' ');
printField(AnalyzerFieldDescriptor{"source.freq.band", "upper_power_max"}, frequencySource.upperPowerMax, 1);
Serial.print(' ');
printField(AnalyzerFieldDescriptor{"source.freq.band", "neighbor_power_mean_max"}, frequencySource.neighborPowerMeanMax, 1);
Serial.print(' ');
printField(AnalyzerFieldDescriptor{"source.freq.band", "neighbor_power_max_max"}, frequencySource.neighborPowerMaxMax, 1);

Serial.print(' ');
printField(AnalyzerFieldDescriptor{"source.freq.band", "target_power_max_ms"}, frequencySource.targetPowerMaxMs);
Serial.print(' ');
printField(AnalyzerFieldDescriptor{"source.freq.band", "lower_power_max_ms"}, frequencySource.lowerPowerMaxMs);
Serial.print(' ');
printField(AnalyzerFieldDescriptor{"source.freq.band", "upper_power_max_ms"}, frequencySource.upperPowerMaxMs);

Serial.print(' ');
printField(AnalyzerFieldDescriptor{"source.freq.band", "lower_score_max"}, frequencySource.lowerScoreMax, 1);
Serial.print(' ');
printField(AnalyzerFieldDescriptor{"source.freq.band", "upper_score_max"}, frequencySource.upperScoreMax, 1);
```

Optional mean fields if line length is acceptable:

```cpp
Serial.print(' ');
printField(AnalyzerFieldDescriptor{"source.freq.band", "target_power_mean"}, frequencySource.targetPowerMean, 1);
Serial.print(' ');
printField(AnalyzerFieldDescriptor{"source.freq.band", "lower_power_mean"}, frequencySource.lowerPowerMean, 1);
Serial.print(' ');
printField(AnalyzerFieldDescriptor{"source.freq.band", "upper_power_mean"}, frequencySource.upperPowerMean, 1);
```

---

## Expected output

Example:

```txt
SEQ_SOURCE_DIAG trial=42 profile=TonalPulse source=FrequencyMatchSource result=miss
source.freq.score.max=486000.0
source.freq.contrast.max=405.0
source.freq.band.target_power_max=932000.0
source.freq.band.lower_power_max=1200.0
source.freq.band.upper_power_max=900.0
source.freq.band.neighbor_power_mean_max=1050.0
source.freq.band.neighbor_power_max_max=1200.0
source.freq.band.target_power_max_ms=123456
source.freq.band.lower_power_max_ms=123448
source.freq.band.upper_power_max_ms=123460
source.freq.band.lower_score_max=620.0
source.freq.band.upper_score_max=480.0
```

---

## Diagnostic interpretation

### Case A — target high, neighbors low

```txt
target_power_max high
lower_power_max low
upper_power_max low
contrast high
```

Meaning:

```txt
Good target-band evidence.
If result is miss, suspect duration gate, candidate lifecycle, release, pattern rejection, or timing.
```

---

### Case B — target high, one neighbor also high

```txt
target_power_max high
lower_power_max high OR upper_power_max high
contrast lower than expected
```

Meaning:

```txt
Frequency energy exists, but dominance is weak.
Could be acoustic coloration, broad-band event, wrong frequency spacing, or speaker/mic resonance.
```

---

### Case C — target low, neighbors low

```txt
target_power_max low
lower_power_max low
upper_power_max low
```

Meaning:

```txt
No useful frequency-band energy in the trial window.
Likely acoustic miss, occlusion, distance, mic/input issue, or wrong timing window.
```

---

### Case D — target low, neighbor high

```txt
target_power_max low
lower_power_max high OR upper_power_max high
```

Meaning:

```txt
Energy is present but not at the expected target band.
Possible target-frequency mismatch, L/R mismatch, wrong emitted frequency, speaker coloration, or neighbor band catching the real tone.
```

This is the important new diagnostic case.

---

## Suggested reason split

The old broad class:

```txt
No Frequency Strength
```

should eventually split into:

```txt
NoFreshBandData
TargetScoreTooLow
ContrastTooLow
NeighborDominated
MatchTooShort
FeatureTooStale
InsufficientCoverage
```

For this pass, printing the band max/mean values is enough. The reason split can come after the first real diagnostic runs.

---

## Minimal patch order

```txt
1. FreqBandStream: store lower/upper power + score.
2. FrequencyBandMeasurementPacket: carry lower/upper fields.
3. FrequencyMeasurementPacketBuilder: copy fields.
4. FrequencyMatchDetector: track max/mean per band.
5. DetectionDiagnostics: expose stats.
6. AnalyzerFrequencyDiagnostic: copy stats.
7. SEQ_SOURCE_DIAG: print source.freq.band.* fields.
```

---

## Architecture rule

Goertzel frame contains band facts.

Detector diagnostics aggregate trial-window facts.

Analyzer prints readable source diagnostics.

Pattern and Behavior should not receive detector-specific band internals.
