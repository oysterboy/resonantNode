# Execution Plan: FrequencyMatch Attack/Release Cleanup

## Scope
Refactor Of FreqencyMatchDetector, and its Providrs and Consumers where necessary.
It does **not** implement the future `FrequencyMatchWindowEvaluator`.
It does **not** implement `ScalarFeatureFrame`.
It does **not** implement real `validWindow` semantics.
It does **not** retune distance/direction behavior yet.

The pass does:

1. Clean up FrequencyFeatureFrame availability naming.
2. Add explicit FrequencyMatch attack/release thresholds.
3. Replace the single `matched` lifecycle gate in `FrequencyMatchDetector`.
4. Rename current/longest match-run variables to diagnostic-only match-streak variables.
5. Preserve contrast-first peak selection.
6. Update runtime/analyzer reporting and banners so tests are interpretable.

## Status Ledger

This is the current read-through status for the pass. `COMPAT` means the code still carries compatibility mirrors while the new shape is in place.

- Scope 1: `DONE / COMPAT`
- Scope 2: `DONE / COMPAT`
- Scope 3: `DONE / COMPAT`
- Scope 4: `DONE / COMPAT`
- Scope 5: `DONE / COMPAT`
- Scope 6: `DONE / COMPAT`

- Phase 1: `DONE / COMPAT`
  - 1.1 `DONE / COMPAT`
  - 1.2 `DONE / COMPAT`
  - 1.3 `DONE / COMPAT`
- Phase 2: `DONE / COMPAT`
  - 2.1 `DONE / COMPAT`
  - 2.2 `DONE / COMPAT`
- Phase 3: `DONE / COMPAT`
  - 3.1 `DONE / COMPAT`
  - 3.2 `DONE / COMPAT`
  - 3.3 `DONE / COMPAT`
- Phase 4: `DONE / COMPAT`
- Phase 5: `DONE / COMPAT`
  - 5.1 `DONE / COMPAT`
  - 5.2 `PARTIAL / COMPAT`
  - 5.3 `DONE / COMPAT`
  - 5.4 `DONE / COMPAT`
- Phase 6: `DONE / PARTIAL / COMPAT`
  - 6.1 `DONE / COMPAT`
  - 6.2 `DONE / COMPAT`
  - 6.3 `DONE / PARTIAL / COMPAT`
  - 6.4 `DONE / COMPAT`
  - 6.5 `DONE / COMPAT`
  - 6.6 `PARTIAL / COMPAT`
  - 6.7 `DONE / COMPAT`
- Phase 7: `PARTIAL / COMPAT`
  - 7.1 `PARTIAL / COMPAT`
  - 7.2 `DONE / COMPAT`
  - 7.3 `PARTIAL / COMPAT`
- Phase 8: `DONE / PARTIAL / COMPAT`
  - 8.1 `DONE / COMPAT`
  - 8.2 `DONE / COMPAT`
  - 8.3 `PARTIAL / COMPAT`
- Phase 9: `DONE / PARTIAL / COMPAT`
- Phase 10: `PARTIAL / COMPAT`
- Phase 11: `PARTIAL`
- Phase 12: `PARTIAL`

---

## Current code facts

### Current files involved

Core detection:

```txt
src/detection/inspector/InspectorTypes.h
src/detection/DetectionProfile.h
src/detection/features/FrequencyMatchEvaluation.h
src/detection/detectors/FrequencyMatchDetector.h
src/detection/detectors/FrequencyMatchDetector.cpp
src/detection/occurrences/FrequencyOccurrenceSource.cpp
src/detection/DetectionRuntime.h
src/detection/DetectionRuntime.cpp
```

Analyzer / reporting:

```txt
src/modes/analyzer/AnalyzerSequenceHelpers.cpp
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerApp.h
src/modes/analyzer/AnalyzerReporting.h
src/modes/analyzer/AnalyzerReporting.cpp
src/modes/analyzer/AnalyzerSequenceSession.cpp
src/modes/analyzer/AnalyzerCommands.cpp
```

Resonant node:

```txt
src/modes/resonant/node.cpp
```

### Current behavior to replace

In `FrequencyMatchDetector.cpp`, the detector currently evaluates:

```cpp
const auto liveFreqEval = FrequencyMatchEvaluation::evaluate(evidence, tuning);
```

and uses:

```cpp
liveFreqEval.matched
```

for too many lifecycle roles:

```txt
open candidate
extend candidate
reset match run
release candidate
diagnostic gate reason
```

Current `matched` means roughly:

```txt
evidence.present && evidence.validWindow && scoreOk && contrastOk
```

This must become diagnostic/compatibility only. Runtime lifecycle should use explicit:

```txt
attackOk
releaseOk
```

---

# Phase 1 — Normalize data field naming first

Do this before detector lifecycle changes.

## 1.1 Update FrequencyFeatureFrame

File:

```txt
src/detection/inspector/InspectorTypes.h
```

Current struct:

```cpp
struct FrequencyFeatureFrame {
    bool present = false;
    bool matched = false;
    bool updatedThisFrame = false;
    ...
    bool windowAvailable = false;
    ...
    bool validWindow = false;
};
```

Change to target shape:

```cpp
struct FrequencyFeatureFrame {
    bool evidencePresent = false;
    bool matched = false;              // keep as compatibility / result flag
    bool updatedThisFrame = false;

    unsigned long targetHz = 0;
    unsigned long observedAtMs = 0;
    uint64_t windowStartSample = 0;
    uint64_t windowEndSample = 0;
    unsigned long windowSampleCount = 0;
    unsigned long ageSamples = 0;

    float score = 0.0f;
    float confidence = 0.0f;

    float targetPower = 0.0f;
    float neighborPower = 0.0f;
    float totalEnergy = 0.0f;
    float spectralContrast = 0.0f;
};
```

Remove:

```cpp
present
windowAvailable
validWindow
```

If removing immediately causes too many compile errors, use a transitional version:

```cpp
bool evidencePresent = false;

// Deprecated aliases. Do not use in new code.
bool present = false;
bool windowAvailable = false;
bool validWindow = false;
```

But the preferred pass is less conservative: remove the aliases and fix compile errors.

## 1.2 Update capture sites

Files:

```txt
src/modes/analyzer/AnalyzerSequenceHelpers.cpp
src/modes/resonant/node.cpp
```

Current pattern:

```cpp
const bool present = _freqBandStream.windowReady();

evidence.present = present;
evidence.windowAvailable = present;
evidence.validWindow = present;
```

Replace with:

```cpp
const bool evidencePresent = _freqBandStream.windowReady();

evidence.evidencePresent = evidencePresent;
```

Keep:

```cpp
evidence.updatedThisFrame = _freqBandStream.updatedOnLastObserve();
evidence.ageSamples = _freqBandStream.evidenceAgeSamples();
```

Add comment at both capture sites or near the struct:

```cpp
// evidencePresent means the frequency analysis window exists.
// It does not mean the evidence was freshly computed.
// Freshness is represented by updatedThisFrame / ageSamples.
```

## 1.3 Update FeatureExtractor

File:

```txt
src/detection/features/FeatureExtractor.h
```

Current:

```cpp
if (!evidence.present) return;
if (!evidence.validWindow || !evidence.updatedThisFrame) return;
```

Replace with:

```cpp
if (!evidence.evidencePresent) return;
if (!evidence.updatedThisFrame) return;
```

Keep the freshness rule: history should record only freshly computed frequency frames.

---

# Phase 2 — Update config types before detector logic

## 2.1 Update FrequencyMatchConfig

File:

```txt
src/detection/DetectionProfile.h
```

Current:

```cpp
struct FrequencyMatchConfig {
    unsigned long releaseDebounceMs = 20;
    unsigned long cooldownAfterOnsetMs = 300;
    unsigned long minTransientDurationMs = 80;
    float scoreMin = 10000.0f;
    float contrastMin = 50.0f;
};
```

Replace with:

```cpp
struct FrequencyMatchConfig {
    unsigned long releaseDebounceMs = 30;
    unsigned long cooldownAfterReleaseMs = 0;
    unsigned long minDurationMs = 60;

    float attackScoreMin = 10000.0f;
    float releaseScoreMin = 8000.0f;

    float attackContrastMin = 50.0f;
    float releaseContrastMin = 50.0f;
};
```

If a compatibility step is needed, keep old names only temporarily:

```cpp
// Deprecated compatibility fields. Do not use in new code.
float scoreMin = attackScoreMin;
float contrastMin = attackContrastMin;
unsigned long minTransientDurationMs = minDurationMs;
unsigned long cooldownAfterOnsetMs = cooldownAfterReleaseMs;
```

Preferred pass: remove old names and fix compile errors.

## 2.2 Update makeTonalPulseProfile()

File:

```txt
src/detection/DetectionProfile.h
```

Replace:

```cpp
profile.frequencyMatch.scoreMin = 8000.0f;
profile.frequencyMatch.contrastMin = 50.0f;
profile.frequencyMatch.minTransientDurationMs = 60;
profile.frequencyMatch.releaseDebounceMs = 30;
profile.frequencyMatch.cooldownAfterOnsetMs = 0;
```

with conservative first test defaults:

```cpp
profile.frequencyMatch.attackScoreMin = 10000.0f;
profile.frequencyMatch.releaseScoreMin = 8000.0f;

profile.frequencyMatch.attackContrastMin = 50.0f;
profile.frequencyMatch.releaseContrastMin = 50.0f;

profile.frequencyMatch.minDurationMs = 60;
profile.frequencyMatch.releaseDebounceMs = 30;
profile.frequencyMatch.cooldownAfterReleaseMs = 0;
```

Later test only after compile/pass stability:

```cpp
profile.frequencyMatch.releaseScoreMin = 7000.0f;
```

Do not lower `attackScoreMin` in this pass.

---

# Phase 3 — Replace FrequencyMatchEvaluation

## 3.1 Replace Values with attack/release thresholds

File:

```txt
src/detection/features/FrequencyMatchEvaluation.h
```

Current:

```cpp
struct Values {
    float scoreMin = 50000.0f;
    float contrastMin = 20.0f;
};
```

Replace with:

```cpp
struct Values {
    float attackScoreMin = 10000.0f;
    float releaseScoreMin = 8000.0f;

    float attackContrastMin = 50.0f;
    float releaseContrastMin = 50.0f;
};
```

## 3.2 Replace Evaluation shape

Current `Evaluation` has:

```cpp
present
validWindow
scoreOk
contrastOk
matched
scoreMin
contrastMin
reason
```

Replace with:

```cpp
enum class Reason {
    None,
    NoEvidence,
    AttackScoreTooLow,
    AttackContrastTooLow,
    AttackScoreAndContrastTooLow,
    ReleaseScoreTooLow,
    ReleaseContrastTooLow,
    ReleaseScoreAndContrastTooLow,
};

struct Evaluation {
    bool evidenceOk = false;

    bool attackScoreOk = false;
    bool attackContrastOk = false;
    bool attackOk = false;

    bool releaseScoreOk = false;
    bool releaseContrastOk = false;
    bool releaseOk = false;

    // Compatibility / strict match-streak gate:
    bool matched = false;

    float score = 0.0f;
    float contrast = 0.0f;

    float attackScoreMin = 0.0f;
    float releaseScoreMin = 0.0f;
    float attackContrastMin = 0.0f;
    float releaseContrastMin = 0.0f;

    Reason attackReason = Reason::None;
    Reason releaseReason = Reason::None;
};
```

Define:

```cpp
out.evidenceOk = evidence.evidencePresent;

out.attackScoreOk = evidence.score >= values.attackScoreMin;
out.attackContrastOk = evidence.spectralContrast >= values.attackContrastMin;
out.attackOk = out.evidenceOk && out.attackScoreOk && out.attackContrastOk;

out.releaseScoreOk = evidence.score >= values.releaseScoreMin;
out.releaseContrastOk = evidence.spectralContrast >= values.releaseContrastMin;
out.releaseOk = out.evidenceOk && out.releaseScoreOk && out.releaseContrastOk;

// Strict match-streak gate. Use attackOk for now.
out.matched = out.attackOk;
```

Use `attackOk` for strict match-streak diagnostics.

## 3.3 Update parseToken compatibility

Keep existing command tokens for now:

```cpp
freqScore=
freqContrast=
```

Map them to attack thresholds.

Recommended:

```cpp
if (strncmp(token, "freqScore=", 10) == 0) {
    values.attackScoreMin = strtof(token + 10, nullptr);
    return true;
}
if (strncmp(token, "freqContrast=", 13) == 0) {
    values.attackContrastMin = strtof(token + 13, nullptr);
    return true;
}
```

Add new tokens:

```cpp
freqAttackScore=
freqReleaseScore=
freqAttackContrast=
freqReleaseContrast=
```

Optional: if only `freqScore=` is provided, do **not** silently change release threshold unless explicitly desired. Better rule:

```txt
freqScore= changes attackScoreMin only.
freqReleaseScore= changes releaseScoreMin.
```

But for older manual testing, it may be convenient to set both. Choose one and document it. Recommended for safety: old tokens set attack only.

---

# Phase 4 — Update FrequencyOccurrenceSource call wiring

File:

```txt
src/detection/occurrences/FrequencyOccurrenceSource.cpp
```

Current:

```cpp
FrequencyMatchEvaluation::Values frequencyTuning = {};
frequencyTuning.scoreMin = _config.scoreMin;
frequencyTuning.contrastMin = _config.contrastMin;

_detector.update(
    evidence,
    frame.sampleTimeMs,
    frame.sampleIndex,
    frequencyTuning,
    _config.releaseDebounceMs,
    _config.cooldownAfterOnsetMs,
    _config.minTransientDurationMs);
```

Replace with:

```cpp
FrequencyMatchEvaluation::Values frequencyTuning = {};
frequencyTuning.attackScoreMin = _config.attackScoreMin;
frequencyTuning.releaseScoreMin = _config.releaseScoreMin;
frequencyTuning.attackContrastMin = _config.attackContrastMin;
frequencyTuning.releaseContrastMin = _config.releaseContrastMin;

_detector.update(
    evidence,
    frame.sampleTimeMs,
    frame.sampleIndex,
    frequencyTuning,
    _config.releaseDebounceMs,
    _config.cooldownAfterReleaseMs,
    _config.minDurationMs);
```

Also update peak-evidence selection:

Current:

```cpp
if (_detector.candidateActive && (!_peakEvidence.present
    || evidence.spectralContrast > _peakEvidence.spectralContrast
    || ...)) {
```

Replace:

```cpp
if (_detector.candidateActive && (!_peakEvidence.evidencePresent
    || evidence.spectralContrast > _peakEvidence.spectralContrast
    || ...)) {
```

When building occurrence:

Current:

```cpp
candidate.frequency.present = true;
candidate.frequency.matched = _detector.frequencyCandidate.valid;
candidate.frequency.windowAvailable = _detector.readyOk;
candidate.frequency.validWindow = _detector.readyOk;
```

Replace:

```cpp
candidate.frequency.evidencePresent = true;
candidate.frequency.matched = _detector.frequencyCandidate.valid;
```

Do not write removed fields.

---

# Phase 5 — Refactor FrequencyMatchDetector.h

File:

```txt
src/detection/detectors/FrequencyMatchDetector.h
```

## 5.1 Rename public diagnostic flags

Current:

```cpp
bool present = false;
float thresholdScore = 0.0f;
float thresholdContrast = 0.0f;
bool readyOk = false;
bool bestScoreOk = false;
bool bestContrastOk = false;
bool gateOpen = false;
```

Replace with clearer names:

```cpp
bool evidencePresent = false;

float attackScoreThreshold = 0.0f;
float releaseScoreThreshold = 0.0f;
float attackContrastThreshold = 0.0f;
float releaseContrastThreshold = 0.0f;

bool evidenceOk = false;
bool attackScoreOk = false;
bool attackContrastOk = false;
bool attackOk = false;
bool releaseScoreOk = false;
bool releaseContrastOk = false;
bool releaseOk = false;
```

If reporting depends heavily on old names, keep old names as aliases for one pass, but update `DetectionRuntime` immediately.

## 5.2 Rename candidate timing fields

Current:

```cpp
candidateFirstSeenMs
candidateLastMatchedMs
candidateReleaseMs
candidateHoldMs
```

Preferred new conceptual names:

```cpp
candidateOpenMs
candidateLastAttackOkMs
candidateLastReleaseOkMs
candidateCloseCheckMs
candidateDurationMs
```

Compatibility option:

Keep old public fields, but internally comment/map them:

```cpp
// Deprecated names:
// candidateFirstSeenMs == candidateOpenMs
// candidateLastMatchedMs == candidateLastReleaseOkMs
// candidateHoldMs == candidateDurationMs
```

Recommended if time is limited: do not rename all candidate fields in this pass. Do rename the *logic* and add comments. Full public rename can be a follow-up.

## 5.3 Rename match-run fields

Current:

```cpp
currentMatchRunFrames
currentMatchRunStartMs
longestMatchRunFrames
longestMatchRunStartMs
longestMatchRunEndMs
```

Rename now:

```cpp
diagCurrentMatchStreakFrames
diagCurrentMatchStreakStartMs
diagLongestMatchStreakFrames
diagLongestMatchStreakStartMs
diagLongestMatchStreakEndMs
```

These must be diagnostic-only.

## 5.4 Add release cause

Add:

```cpp
enum class FrequencyReleaseFailCause {
    None,
    NoEvidence,
    ScoreLow,
    ContrastLow,
    ScoreAndContrastLow
};
```

Add members:

```cpp
FrequencyReleaseFailCause lastReleaseFailCause = FrequencyReleaseFailCause::None;
FrequencyReleaseFailCause candidateCloseCause = FrequencyReleaseFailCause::None;
```

Add helper declaration:

```cpp
const char* releaseFailCauseName(FrequencyReleaseFailCause cause);
```

or a private static helper in `.cpp`.

---

# Phase 6 — Refactor FrequencyMatchDetector.cpp lifecycle

File:

```txt
src/detection/detectors/FrequencyMatchDetector.cpp
```

This is the central step.

## 6.1 First: update resetState()

Reset all new fields:

```cpp
evidencePresent = false;

attackScoreThreshold = 0.0f;
releaseScoreThreshold = 0.0f;
attackContrastThreshold = 0.0f;
releaseContrastThreshold = 0.0f;

evidenceOk = false;
attackScoreOk = false;
attackContrastOk = false;
attackOk = false;
releaseScoreOk = false;
releaseContrastOk = false;
releaseOk = false;

diagCurrentMatchStreakFrames = 0;
diagCurrentMatchStreakStartMs = 0;
diagLongestMatchStreakFrames = 0;
diagLongestMatchStreakStartMs = 0;
diagLongestMatchStreakEndMs = 0;

lastReleaseFailCause = FrequencyReleaseFailCause::None;
candidateCloseCause = FrequencyReleaseFailCause::None;
```

## 6.2 Add diagnostic match-streak helpers

Add private helper or local repeated block:

```cpp
void updateDiagMatchStreak(bool strictMatchOk, unsigned long now) {
    if (strictMatchOk) {
        if (diagCurrentMatchStreakFrames == 0) {
            diagCurrentMatchStreakStartMs = now;
        }
        ++diagCurrentMatchStreakFrames;
        if (diagCurrentMatchStreakFrames > diagLongestMatchStreakFrames) {
            diagLongestMatchStreakFrames = diagCurrentMatchStreakFrames;
            diagLongestMatchStreakStartMs = diagCurrentMatchStreakStartMs;
            diagLongestMatchStreakEndMs = now;
        }
    } else {
        diagCurrentMatchStreakFrames = 0;
        diagCurrentMatchStreakStartMs = 0;
    }
}
```

Important: this helper must not open/close/emit candidates.

## 6.3 Rewrite update() shape

Current `update()` has nested:

```cpp
if (evidence.present) {
    if (liveFreqEval.matched) {
        open/extend
    } else if (currentMatchRunFrames > 0) {
        reset run
    } else if (candidateActive...) {
        release
    }
}
```

Replace with linear lifecycle:

```cpp
const auto gates = FrequencyMatchEvaluation::evaluate(evidence, tuning);

evidencePresent = evidence.evidencePresent;
evidenceOk = gates.evidenceOk;

attackScoreThreshold = tuning.attackScoreMin;
releaseScoreThreshold = tuning.releaseScoreMin;
attackContrastThreshold = tuning.attackContrastMin;
releaseContrastThreshold = tuning.releaseContrastMin;

attackScoreOk = gates.attackScoreOk;
attackContrastOk = gates.attackContrastOk;
attackOk = gates.attackOk;

releaseScoreOk = gates.releaseScoreOk;
releaseContrastOk = gates.releaseContrastOk;
releaseOk = gates.releaseOk;

thresholdScore = attackScoreThreshold;       // compatibility if kept
thresholdContrast = attackContrastThreshold; // compatibility if kept
readyOk = evidenceOk;                        // compatibility if kept
bestScoreOk = attackScoreOk;                 // compatibility if kept
bestContrastOk = attackContrastOk;           // compatibility if kept
gateOpen = attackOk;                         // compatibility if kept

updateDiagMatchStreak(gates.attackOk, now);
```

Then candidate lifecycle:

```cpp
if (!candidateActive) {
    if (gates.attackOk) {
        if (timing::beforeDeadline(now, candidateRefractoryUntilMs)) {
            setGateReason("refractory");
            return_after_diagnostics;
        }

        openCandidateFromEvidence(evidence, now, currentSample);
    }

    updateDiagnosticsFromEvidence(...);
    return;
}

if (candidateActive) {
    if (gates.releaseOk) {
        candidateLastMatchedMs = now; // or candidateLastReleaseOkMs if renamed
        candidateHoldMs = now >= candidateFirstSeenMs ? now - candidateFirstSeenMs : 0;
        candidateHoldWindows++;
        updatePeakEvidenceIfBetter(evidence, now, currentSample);
        frequencyCandidate.durationMs = candidateHoldMs;
    } else {
        lastReleaseFailCause = classifyReleaseFail(gates, evidence);

        if (timing::elapsedSince(now, candidateLastMatchedMs, releaseDebounceMs)) {
            closeCandidate(now, currentSample, minTransientDurationMs, cooldownAfterReleaseMs);
        }
    }

    updateDiagnosticsFromEvidence(...);
}
```

## 6.4 Candidate duration rule

Keep current semantic:

```txt
duration = last release-ok time - candidate open time
```

Do not count the debounce tail as valid duration.

Current equivalent:

```cpp
candidateReleaseMs = candidateLastMatchedMs;
candidateHoldMs = candidateReleaseMs - candidateFirstSeenMs;
```

After refactor, keep:

```cpp
candidateReleaseMs = candidateLastMatchedMs; // now means lastReleaseOkMs
candidateHoldMs = candidateReleaseMs >= candidateFirstSeenMs
    ? candidateReleaseMs - candidateFirstSeenMs
    : 0UL;
```

Add comment:

```cpp
// Duration is measured to the last release-ok evidence, not to the debounced close-check time.
```

## 6.5 Close cause / noEmitReason

When closing:

```cpp
const bool holdOk = candidateHoldMs >= minDurationMs;
candidateEmitted = holdOk;
validRelease = holdOk;
emitAllowed = holdOk;

if (holdOk) {
    noEmitReason = "none";
} else {
    noEmitReason = "duration_too_short";
}
```

Add close cause separately:

```cpp
candidateCloseCause = lastReleaseFailCause;
```

Do not replace `duration_too_short`; it is still the final reject reason.
Add a new diagnostic string:

```txt
close_cause=score_low|contrast_low|score_and_contrast_low|no_evidence
```

## 6.6 Diagnostics counters

Existing counters:

```cpp
diagnosticsScoreOkCount
diagnosticsContrastOkCount
diagnosticsBothOkCount
diagnosticsScoreTooLowCount
diagnosticsContrastTooLowCount
diagnosticsScoreAndContrastTooLowCount
```

Keep them, but decide which gate they refer to.

For continuity with old logs, keep them as **attack gate counters**:

```txt
score_ok_frames = attackScoreOk frames
contrast_ok_frames = attackContrastOk frames
both_ok_frames = attackOk frames
```

Add new release counters:

```cpp
diagnosticsReleaseScoreOkCount
diagnosticsReleaseContrastOkCount
diagnosticsReleaseBothOkCount
diagnosticsReleaseScoreTooLowCount
diagnosticsReleaseContrastTooLowCount
diagnosticsReleaseScoreAndContrastTooLowCount
diagnosticsReleaseNoEvidenceCount
```

If too much for one pass, add only:

```cpp
diagnosticsReleaseOkCount
diagnosticsReleaseFailScoreLowCount
diagnosticsReleaseFailContrastLowCount
diagnosticsReleaseFailBothLowCount
diagnosticsReleaseFailNoEvidenceCount
```

## 6.7 Best evidence / peak behavior

Keep contrast-first behavior exactly.

In candidate peak:

```cpp
if (evidence.spectralContrast > candidatePeakContrast
    || (evidence.spectralContrast == candidatePeakContrast && evidence.score > candidatePeakScore)) {
    updatePeakEvidence();
}
```

In diagnostics best evidence:

```cpp
if (!bestEvidence.evidencePresent
    || evidence.spectralContrast > bestContrast
    || (evidence.spectralContrast == bestContrast && evidence.score > bestScore)) {
    bestEvidence = evidence;
}
```

Do not make score primary.

---

# Phase 7 — Update DetectionRuntime diagnostics

Files:

```txt
src/detection/DetectionRuntime.h
src/detection/DetectionRuntime.cpp
```

## 7.1 Rename longest run diagnostics

Current fields:

```cpp
frequencyLongestMatchRunFrames
frequencyLongestMatchRunStartMs
frequencyLongestMatchRunEndMs
```

Rename to:

```cpp
frequencyDiagLongestMatchStreakFrames
frequencyDiagLongestMatchStreakStartMs
frequencyDiagLongestMatchStreakEndMs
```

If AnalyzerReporting is too coupled, keep printed field names temporarily but source from new detector fields.

Update assignments:

```cpp
_diagnostics.frequencyDiagLongestMatchStreakFrames = detector.diagLongestMatchStreakFrames;
_diagnostics.frequencyDiagLongestMatchStreakStartMs = detector.diagLongestMatchStreakStartMs;
_diagnostics.frequencyDiagLongestMatchStreakEndMs = detector.diagLongestMatchStreakEndMs;
```

## 7.2 Update threshold diagnostics

Current:

```cpp
frequencyScoreThreshold = detector.thresholdScore;
frequencyContrastThreshold = detector.thresholdContrast;
```

Replace / extend:

```cpp
frequencyAttackScoreThreshold = detector.attackScoreThreshold;
frequencyReleaseScoreThreshold = detector.releaseScoreThreshold;
frequencyAttackContrastThreshold = detector.attackContrastThreshold;
frequencyReleaseContrastThreshold = detector.releaseContrastThreshold;
```

If only one threshold can be printed now, print attack threshold as the old field and add release fields separately.

## 7.3 Source summary

Keep:

```cpp
sourceSummary.bestDurationMs
sourceSummary.secondBestDurationMs
sourceSummary.totalMatchMs
sourceSummary.totalGapMs
sourceSummary.maxGapMs
```

Add:

```cpp
sourceSummary.closeCause = detector.candidateCloseCauseName();
sourceSummary.releaseOkFrames = detector.diagnosticsReleaseOkCount;
```

If struct changes are too large, print close cause via `frequencyGateReason` or `sourceLastCandidate.gateReason` temporarily.

---

# Phase 8 — Update Analyzer reporting and banners

Files:

```txt
src/modes/analyzer/AnalyzerReporting.cpp
src/modes/analyzer/AnalyzerReporting.h
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerSequenceSession.cpp
src/modes/analyzer/AnalyzerCommands.cpp
```

## 8.1 Banner

Current banner prints:

```txt
freq_score_min
freq_contrast_min
```

Change to:

```txt
freq_attack_score_min
freq_release_score_min
freq_attack_contrast_min
freq_release_contrast_min
```

Also keep:

```txt
freq_min_duration_ms
freq_release_debounce_ms
freq_cooldown_ms
```

Use `cooldown_ms` but source it from `cooldownAfterReleaseMs`.

## 8.2 Help / params

Current help:

```txt
PARAM freqScore=10000 freqContrast=50.0
```

Keep for compatibility but mark as attack threshold:

```txt
PARAM freqScore=10000 freqContrast=50.0
PARAM freqReleaseScore=8000 freqReleaseContrast=50.0
```

Update parser:

```txt
freqScore -> attackScoreMin
freqContrast -> attackContrastMin
freqReleaseScore -> releaseScoreMin
freqReleaseContrast -> releaseContrastMin
```

## 8.3 Source diag output

Keep old field names if needed:

```txt
score_ok_frames
contrast_ok_frames
both_ok_frames
```

but add clear new fields:

```txt
attack_ok_frames
release_ok_frames
release_fail_score_low_frames
release_fail_contrast_low_frames
release_fail_both_low_frames
release_fail_no_evidence_frames
close_cause
```

Rename printed run field later if too much:

```txt
longest_match_streak_frames
longest_match_streak_ms
```

Do not call it `longest_run` if changing output is easy.

---

# Phase 9 — Update Resonant node commands/status

File:

```txt
src/modes/resonant/node.cpp
```

Current status/help uses:

```txt
freqMatch.scoreMin
freqMatch.contrastMin
freqScore
freqContrast
```

Update to:

```txt
freqMatch.attackScoreMin
freqMatch.releaseScoreMin
freqMatch.attackContrastMin
freqMatch.releaseContrastMin
```

For commands, keep old user params:

```txt
RB PARAM freqScore=10000 freqContrast=50.0
```

mapped to attack thresholds.

Add:

```txt
RB PARAM freqReleaseScore=8000 freqReleaseContrast=50.0
```

Do not make behavior code depend on release diagnostics.

---

# Phase 10 — Compile-driven cleanup checklist

After phases 1–9, compile and fix errors in this order:

1. `FrequencyFeatureFrame.present` missing.
   - Replace with `evidencePresent`.
2. `windowAvailable` / `validWindow` missing.
   - Remove or replace with `evidencePresent`.
3. `scoreMin` / `contrastMin` missing.
   - Replace with attack fields.
4. `cooldownAfterOnsetMs` missing.
   - Replace with `cooldownAfterReleaseMs`.
5. `minTransientDurationMs` missing.
   - Replace with `minDurationMs`.
6. `currentMatchRun*` / `longestMatchRun*` missing.
   - Replace with `diagCurrentMatchStreak*` / `diagLongestMatchStreak*`.
7. Analyzer report struct field mismatches.
   - Either rename report fields or map new fields to old printed names temporarily.

---

# Phase 11 — First test sequence

## Test 0 — compile parity / no tuning change

Before testing softer release, set:

```cpp
attackScoreMin = 10000.0f;
releaseScoreMin = 10000.0f;

attackContrastMin = 50.0f;
releaseContrastMin = 50.0f;

minDurationMs = 60;
releaseDebounceMs = 30;
cooldownAfterReleaseMs = 0;
```

Goal: behavior should be close to old `score=10000 / contrast=50 / min=60 / debounce=30`.

This catches refactor bugs.

## Test A — conservative hysteresis

```cpp
attackScoreMin = 10000.0f;
releaseScoreMin = 8000.0f;

attackContrastMin = 50.0f;
releaseContrastMin = 50.0f;

minDurationMs = 60;
releaseDebounceMs = 30;
cooldownAfterReleaseMs = 0;
```

## Test B — permissive hysteresis

```cpp
attackScoreMin = 10000.0f;
releaseScoreMin = 7000.0f;

attackContrastMin = 50.0f;
releaseContrastMin = 50.0f;

minDurationMs = 60;
releaseDebounceMs = 30;
cooldownAfterReleaseMs = 0;
```

Only after these:

```cpp
releaseDebounceMs = 40;
minDurationMs = 50;
```

Do not combine all changes at once.

---

# Phase 12 — Acceptance criteria

Pass is good if:

```txt
- compile succeeds
- banner shows attack/release thresholds
- SEQ_SOURCE_REJECTS still prints best/second/total/max_gap
- match-streak diagnostics are still visible
- runtime candidate lifecycle no longer reads current/longest match-streak variables
- misses decrease or rejected candidates show longer best_dur_ms under release hysteresis
- duplicates/unexpected do not increase
- off-frequency sine and handclaps do not create accepted false positives
```

Failure signs:

```txt
- accepted durations become absurdly long
- duplicate rate rises
- unexpected detections appear
- handclaps/off-target sine pass
- close_cause always unknown
- release_ok_frames not printed or always zero
```

---

# Important implementation rule

Do not mix the later retrospective evaluator into this pass.

The old high-hit-rate concept belongs to a later Analyzer-only path:

```txt
FrequencyMatchWindowEvaluator / SustainMin mode
```

This pass is only the live runtime detector cleanup.

Runtime detector decides via:

```txt
attackOk
releaseOk
releaseDebounceMs
minDurationMs
```

Analyzer later may evaluate:

```txt
longest match streak in trial window
```

but that must remain separate.
