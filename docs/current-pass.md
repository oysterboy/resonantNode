# Analyzer Refactor — Pass C: Build AnalyzerReport from Current Trial

**Project:** ResonantNode / Resonanzraum  
**Area:** Detection Refactor / Analyzer  
**Pass:** C  
**Goal:** Create an `AnalyzerReport` during SEQ trial finalization by mapping the current Analyzer trial data into the new stable reporting model.

Status: Pass C is complete. The next active pass is Pass D.

---

## 0. Context

Pass A quarantines legacy output modes and clarifies `SEQ_EXPLAIN` versus actual RAW sample capture.

Pass B adds:

```txt
AnalyzerReporting.h
AnalyzerResult
AnalyzerReason
AnalyzerReport
AnalyzerRunContext
AnalyzerPatternObservation
AnalyzerSignalObservation
AnalyzerInspectionObservation
AnalyzerFieldObservation
AnalyzerClassification
AnalyzerProfileDetail
AnalyzerDebugSummary
```

Pass C should start **using** those structs, but it should not yet replace the visible output format.

---

## 1. Core intent

Create a bridge from the current Analyzer code to the future reporting model.

Current Analyzer still computes trial results using existing `SequenceTest` state and diagnostics.

Pass C should add a builder/helper that creates:

```txt
AnalyzerReport
```

from the currently available data.

This report will later drive:

```txt
Pass D — new default SEQ_TRIAL
Pass E — SEQ_EXPLAIN
Pass F — SEQ_SUMMARY cleanup
```

---

## 2. Non-goals

Do not change detection behavior.

Do not change thresholds.

Do not change classification behavior.

Do not replace visible `SEQ_TRIAL` output yet.

Do not remove legacy output.

Do not touch actual RAW sample capture.

Do not introduce `AudioReporting.h` yet.

Do not refactor Runtime Behavior.

---

## 3. Files to inspect

Start with:

```txt
src/modes/analyzer/AnalyzerApp.h
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerReporting.h
```

Likely relevant current areas:

```txt
AnalyzerApp::finalizeSequenceTrial(...)
AnalyzerApp::printSequenceTrialResult(...)
AnalyzerApp::printSequenceTrialDebug(...)
AnalyzerApp::printSequenceSummary(...)
SequenceTest::TrialDiagnostics
SequenceTest::TrialReport
evaluateRoadmapSignalCandidate(...)
evaluateRoadmapSignalCandidateImpl(...)
```

Also inspect, but avoid changing unless necessary:

```txt
src/detection/patterns/PatternResult.h
src/detection/signals/SignalCandidate.h
src/detection/signals/InspectedSignal.h
src/detection/field/FieldState.h
```

---

## 4. Add builder function declaration

Add a private helper to `AnalyzerApp`.

Suggested declaration in `AnalyzerApp.h`:

```cpp
AnalyzerReport buildSequenceAnalyzerReport(
    unsigned long trialNumber,
    const char* legacyResult,
    long dtMs,
    long durMs,
    float strength,
    bool audioInvalid,
    unsigned int duplicateCount,
    const SequenceTest::TrialDiagnostics& diagnostics
) const;
```

Adjust parameter types to match existing code.

If the current finalized trial data is already bundled differently, adapt the signature.

The important point:

```txt
The builder should consume existing finalized trial facts.
It should not recompute detection.
```

---

## 5. Add active profile helper

Add a simple helper:

```cpp
const char* activeAnalyzerProfileName() const;
```

Initial implementation may return a fixed or mode-derived value:

```txt
FreqAmp
FreqAmpLiveOnly
AmpTransient
unknown
```

Use the best available current distinction.

Do not implement full profile switching in this pass.

Minimum acceptable:

```cpp
const char* AnalyzerApp::activeAnalyzerProfileName() const {
    return "FreqAmp";
}
```

Only use `"unknown"` if there is genuinely no reliable current profile name.

---

## 6. Map legacy result to AnalyzerResult

In the builder, map current result strings or states into the new enum.

Expected first mapping:

```txt
"expected"      → AnalyzerResult::Expected
"late"          → AnalyzerResult::Late
"miss"          → AnalyzerResult::Miss
"unexpected"    → AnalyzerResult::Unexpected
"invalid_audio" → AnalyzerResult::InvalidAudio
```

If current code has no string but uses booleans/counters, map those.

If duplicate is currently a separate count, do not necessarily replace the primary result with `Duplicate`.

Recommended:

```txt
Primary result remains expected/late/miss/etc.
duplicateCount is stored in debug.duplicates.
Reason may become DuplicatePatternAfterPrimary only if duplicate is the main classification.
```

For now:

```txt
duplicateCount > 0 → report.debug.duplicates = duplicateCount
```

Pass F/G can refine duplicate classification later.

---

## 7. Map AnalyzerReason

Add a helper if useful:

```cpp
AnalyzerReason inferAnalyzerReason(
    AnalyzerResult result,
    const SequenceTest::TrialDiagnostics& diagnostics,
    unsigned int duplicateCount
);
```

Initial mapping:

```txt
Expected
→ ValidPatternInExpectedWindow

Late
→ ValidPatternAfterWindow

Miss + no candidates/signals
→ NoSignalCandidate

Miss + candidates/signals/rejections present
→ SignalSeenButRejected or PatternCandidateRejected

Unexpected
→ UnexpectedValidPatternWithoutTrigger

InvalidAudio
→ InvalidAudio

Duplicate as main classification, if used
→ DuplicatePatternAfterPrimary

Unknown
→ Unknown
```

Keep this conservative.

If unsure between `SignalSeenButRejected` and `PatternCandidateRejected`, prefer:

```txt
SignalSeenButRejected
```

unless the current diagnostics clearly show a pattern candidate was rejected.

---

## 8. Fill AnalyzerRunContext

Populate:

```txt
profileName
trial
mode
targetHz
expectedPattern
expectedWindowStartMs
expectedWindowEndMs
nowMs
```

Suggested defaults:

```txt
mode = "SEQ"
expectedPattern = "neighbor_chirp" or "chirp" if that is the current SEQ target
targetHz = current target frequency if available
expected window = current SEQ timing window if available
nowMs = millis() or current finalization time
```

If fields are not easily available yet, use safe defaults from `AnalyzerReporting.h`.

Do not add new global state just to fill these fields.

---

## 9. Fill AnalyzerClassification

Populate:

```cpp
report.classification.result = mappedResult;
report.classification.reason = inferredReason;
report.classification.dtMs = dtMs;
report.classification.confidence = report.primaryPattern.confidence;
```

If confidence is not available yet:

```txt
confidence = 0.0f
```

Do not invent confidence.

---

## 10. Fill AnalyzerPatternObservation

Use the best current source.

Preferred source:

```txt
current PatternResult from evaluateRoadmapSignalCandidate(...)
```

Useful fields from `PatternResult` may include:

```txt
valid / accepted
type
kind
reasonCode
rejectReason
confidence
locality
ampSupport
source
behaviorEligible
tonalValid
candidateValid
```

Add a small local helper if useful:

```cpp
AnalyzerPatternObservation makeAnalyzerPatternObservation(
    const detection::PatternResult& pattern,
    long dtMs
);
```

Initial mapping should be generic:

```txt
type        = pattern type/kind name if available, otherwise "unknown" or "none"
accepted    = pattern valid/accepted
confidence  = pattern confidence
dtMs        = trial dt
locality    = pattern locality name if available, otherwise "unknown"
sourceClass = pattern source name if available, otherwise "unknown"
reason      = pattern reason/reject reason if available, otherwise "none"
```

If no PatternResult is available in the current path yet:

```txt
type = "none"
accepted = result == Expected or result == Late or result == Unexpected
confidence = 0.0
dtMs = dtMs
reason = analyzerReasonName(report.classification.reason)
```

But prefer using the existing roadmap PatternResult where possible.

---

## 11. Fill AnalyzerSignalObservation

Populate from `SequenceTest::TrialDiagnostics` if available.

Suggested fields:

```txt
total
accepted
rejected
primarySource
primaryDtMs
primaryDurationMs
primaryStrength
primaryConfidence
mainRejectReason
duplicateRisk
```

Safe initial mapping:

```txt
total = diagnostics.candidateCount or candidate count equivalent
accepted = 1 if primary valid candidate exists, else 0
rejected = total - accepted if safe
primaryDtMs = dtMs
primaryDurationMs = durMs
primaryStrength = strength
duplicateRisk = duplicateCount > 0
```

If the current diagnostics do not clearly distinguish signal/candidate/pattern, use the closest existing candidate counts and document this with a comment.

---

## 12. Fill AnalyzerInspectionObservation

Populate minimal fields.

Suggested first mapping:

```txt
inspected = diagnostics.candidateCount or inspected count if available
accepted = 1 if pattern accepted
rejected = inspected - accepted if safe
primaryEvidence = "freq_amp" / "amp" / "frequency" / "unknown"
locality = same as primaryPattern.locality
supportClass = current amp/frequency support class if available
mainRejectReason = inferred reject reason if any
```

If not available:

```txt
primaryEvidence = "unknown"
locality = "unknown"
supportClass = "unknown"
mainRejectReason = "none"
```

Do not overfit this pass to current FreqAmp internals.

---

## 13. Fill AnalyzerFieldObservation

If current Analyzer has no FieldState available, leave defaults:

```txt
state = "unknown"
activity = 0.0
density = 0.0
recentValidPatterns = 0
recentRejects = 0
```

Do not invent a FieldState bridge in this pass.

If FieldState is available cheaply and cleanly, map:

```txt
quiet / active / busy / dense
activity
density
recent valid patterns
recent rejects
```

But this is optional.

---

## 14. Fill AnalyzerProfileDetail

Keep this minimal.

If Pass B used string-only `AnalyzerProfileDetail`:

```txt
namespaceName = "freq_amp" or "none"
summary = ""
```

If detailed fields exist:

```txt
freqScore
freqContrast
ampLevel
ampBase
ampLift
ampNorm
ampLocality
```

populate only when already available in diagnostics.

Do not add expensive computation.

Do not widen the report model for profile-specific details in this pass.

---

## 15. Fill AnalyzerDebugSummary

Populate:

```txt
signals
inspected
patterns
rejects
duplicates
unexpected
mainRejectReason
```

Safe initial mapping:

```txt
signals = report.signals.total
inspected = report.inspection.inspected
patterns = report.primaryPattern.accepted ? 1 : 0
rejects = report.signals.rejected + report.inspection.rejected if safe
duplicates = duplicateCount
unexpected = result == Unexpected ? 1 : 0
mainRejectReason = analyzerReasonName(report.classification.reason) if rejected/miss
```

Keep it simple.

---

## 16. Store or expose the report

Preferred minimal step:

```txt
Build the AnalyzerReport in finalizeSequenceTrial().
Use it only for internal validation or optional debug for now.
Do not replace visible output yet.
```

Options:

### Option A — Local only

Create report locally in `finalizeSequenceTrial()` and do not store it.

This is acceptable if Pass D will immediately use it.

### Option B — Store last report

Add:

```cpp
AnalyzerReport lastAnalyzerReport;
```

to Analyzer app state if useful.

### Option C — Store per trial

Only do this if current code already stores per-trial reports and it is easy.

Do not introduce complex allocation in Pass C.

Recommendation:

```txt
Use local report first, then pass it to output in Pass D.
```

If the compiler complains about unused variables, add a temporary guarded debug or `(void)report;`.

---

## 17. Add comments for transitional mapping

Add a comment near the builder:

```cpp
// Transitional bridge:
// This maps the existing SequenceTest diagnostics and legacy result classification
// into the new AnalyzerReport shape. Later passes will make SEQ_TRIAL,
// SEQ_EXPLAIN, and SEQ_SUMMARY print from AnalyzerReport directly.
```

Add comments where signal/inspection fields are approximate:

```cpp
// Current Analyzer diagnostics do not yet distinguish all signal/inspection layers.
// This field is mapped from existing candidate counts until the pipeline exposes
// a dedicated debug snapshot.
```

---

## 18. Do not change visible output yet

Pass C should compile and run with the same user-visible output as before.

If absolutely necessary to test, add a temporary disabled block:

```cpp
#if 0
printAnalyzerReportDebug(report);
#endif
```

But do not leave noisy new output enabled.

Pass D owns visible `SEQ_TRIAL` changes.

---

## 19. Success criteria

Pass C is successful if:

```txt
Code compiles.
SEQ tests still run.
Visible SEQ output is unchanged or only trivially unchanged.
Actual RAW trigger/sample capture is untouched.
AnalyzerReport is built from finalized trial data.
AnalyzerResult and AnalyzerReason are populated.
Profile name is populated at least minimally.
Primary PatternObservation is populated from PatternResult where available.
Duplicate count is represented in DebugSummary.
No thresholds or detection behavior changed.
```

---

## 20. Quick implementation checklist

```txt
[x] Include AnalyzerReporting.h in AnalyzerApp.
[x] Add activeAnalyzerProfileName().
[x] Add buildSequenceAnalyzerReport(...).
[x] Map legacy result → AnalyzerResult.
[x] Infer AnalyzerReason conservatively.
[x] Fill RunContext.
[x] Fill Classification.
[x] Fill PatternObservation.
[x] Fill SignalObservation with current diagnostics.
[x] Fill InspectionObservation minimally.
[x] Leave FieldObservation unknown unless already available.
[x] Fill DebugSummary.
[x] Build report in finalizeSequenceTrial().
[x] Do not change default output yet.
[x] Compile.
[x] Run/compile-check one short SEQ path.
[x] Confirm RAW trigger path untouched.
```

---

## 21. Expected final state of Pass C

After this pass, the Analyzer still behaves the same externally, but internally it now has a stable report object:

```txt
AnalyzerReport
```

created from each finalized SEQ trial.

This prepares Pass D:

```txt
Print the new compact default SEQ_TRIAL from AnalyzerReport.
```
