# Analyzer Refactor — Pass I: Actual Pipeline Result Handoff

**Project:** ResonantNode / Resonanzraum  
**Area:** Detection Refactor / Analyzer / DetectionRuntime Boundary  
**Pass:** I  
**Goal:** Make the Analyzer consume actual DetectionRuntime pipeline results instead of relying on Analyzer-side re-evaluation as the primary source of pattern meaning.

---

## 0. Context

Previous passes A–H cleaned the Analyzer reporting surface:

```txt
SEQ_TRIAL = compact truth
SEQ_EXPLAIN = why/how
SEQ_SUMMARY = run comparison
legacy output = quarantined
profile reporting = stable
```

But the data source may still be transitional:

```txt
handleSequenceCandidate()
→ live candidate enters Analyzer bookkeeping
→ evaluateRoadmapSignalCandidateImpl()
→ Analyzer reconstructs inspected/pattern result from current feature history
```

This pass starts correcting that boundary.

Target:

```txt
DetectionRuntime produces SignalCandidate / InspectedSignal / PatternResult / FieldState.
Analyzer consumes those actual pipeline results.
Analyzer only classifies trial timing/result.
```

Rule:

```txt
Detection owns meaning.
Analyzer owns measurement/classification.
```

---

## 1. Core intent

Expose or hand off the actual result produced by the detection pipeline at the time it happens.

Analyzer should receive or be able to read:

```txt
PatternResult
optional SignalCandidate / InspectedSignal debug snapshot
FieldState
profile name
timing information
```

without rerunning:

```txt
evaluateRoadmapSignalCandidateImpl()
```

as the primary interpretation path.

---

## 2. Non-goals

Do not remove `evaluateRoadmapSignalCandidateImpl()` yet.

Do not change detection thresholds.

Do not change detector behavior.

Do not change pattern rules.

Do not change behavior/RB logic.

Do not rewrite SEQ output format.

Do not remove legacy output.

Do not touch actual RAW sample capture.

Do not introduce a large generic reporting framework.

---

## 3. Files to inspect

Analyzer side:

```txt
src/modes/analyzer/AnalyzerApp.h
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerReporting.h
```

Detection side:

```txt
src/detection/DetectionRuntime.*
src/detection/DetectionProfile.h
src/detection/patterns/PatternResult.h
src/detection/patterns/*
src/detection/signals/SignalCandidate.h
src/detection/signals/InspectedSignal.h
src/detection/signals/*
src/detection/field/FieldState.h
```

Search for:

```txt
handleSequenceCandidate
evaluateRoadmapSignalCandidateImpl
evaluateRoadmapSignalCandidate
PatternResult
InspectedSignal
SignalCandidate
DetectionRuntime::update
fieldState
profile
```

---

## 4. Identify current live handoff point

Find where the live candidate currently enters Analyzer trial bookkeeping:

```txt
handleSequenceCandidate(...)
```

Document what this function currently receives:

```txt
timing
duration
strength
candidate/source flags
frequency facts
amp facts
raw/transient facts
```

Then identify what is missing compared to the target:

```txt
actual PatternResult
actual InspectedSignal
pattern accepted/rejected reason
confidence
locality/source class
FieldState snapshot
```

Add a code comment if helpful:

```cpp
// Current transitional handoff: Analyzer receives a live candidate-like event,
// not the actual PatternResult produced by DetectionRuntime.
```

---

## 5. Add a pipeline result handoff struct

Add a small boundary struct if no suitable one exists.

Possible location:

```txt
src/detection/DetectionRuntime.h
```

or, if you want to keep Analyzer-specific dependency out of detection:

```txt
src/detection/DetectionPipelineSnapshot.h
```

Suggested minimal struct:

```cpp
struct DetectionPipelineResult {
    bool hasPattern = false;
    detection::PatternResult pattern;

    bool hasSignal = false;
    detection::SignalCandidate signal;

    bool hasInspectedSignal = false;
    detection::InspectedSignal inspectedSignal;

    bool hasField = false;
    detection::FieldState field;

    const char* profileName = "unknown";
    unsigned long timestampMs = 0;
};
```

Adjust namespaces/types to match the current codebase.

If copying large structs is too expensive, use references/pointers or a compact snapshot instead.

Keep it simple and low-risk.

---

## 6. Prefer snapshot over ownership transfer

This should be an observational handoff.

DetectionRuntime remains owner of detection processing.

Analyzer observes a snapshot.

Avoid making Analyzer mutate detection state.

Preferred semantics:

```txt
DetectionRuntime produces result.
Analyzer receives const snapshot.
Analyzer classifies trial.
```

Avoid:

```txt
Analyzer asks DetectionRuntime to re-run detection.
Analyzer modifies PatternResult.
Analyzer owns PatternResult lifecycle.
```

---

## 7. Expose latest result from DetectionRuntime

Choose the least invasive path.

Options:

### Option A — Callback/event handoff

DetectionRuntime calls Analyzer or a listener when a PatternResult is produced.

Example concept:

```cpp
onPatternResult(const DetectionPipelineResult& result);
```

Use only if the architecture already supports callbacks or event sinks.

### Option B — Poll latest result

DetectionRuntime stores the latest result:

```cpp
bool hasLatestPipelineResult() const;
const DetectionPipelineResult& latestPipelineResult() const;
```

Analyzer reads it during/after candidate handling.

This is often lower risk.

### Option C — Pass result through existing handleSequenceCandidate

Extend the existing handoff function to include the result:

```cpp
handleSequenceCandidate(..., const DetectionPipelineResult* pipelineResult)
```

Use this only if the call chain is easy to update.

---

## 8. Recommended first implementation

Prefer a minimal snapshot/polling approach unless current architecture clearly favors events.

Suggested first target:

```txt
DetectionRuntime stores latest PatternResult snapshot.
Analyzer can retrieve it when the candidate/trial finalizes.
```

This avoids major call-chain rewrites.

---

## 9. Build AnalyzerReport from actual PatternResult when available

Update the Pass C builder path so it prefers the actual pipeline result:

```txt
if actual pipeline PatternResult is available:
    fill AnalyzerPatternObservation from actual PatternResult
else:
    fallback to evaluateRoadmapSignalCandidateImpl()
```

Important:

```txt
Fallback remains for now.
Pass J will compare both.
Pass K will remove fallback after parity is proven.
```

Add a clear source marker in comments or report detail:

```txt
pattern_source=actual_pipeline
pattern_source=analyzer_recheck
```

If there is already `AnalyzerProfileDetail`, add a temporary detail field or debug string if useful.

---

## 10. Preserve existing output format

Do not change `SEQ_TRIAL`, `SEQ_EXPLAIN`, or `SEQ_SUMMARY` field order in this pass.

Only change the data source behind `AnalyzerReport.primaryPattern` when actual pipeline result is available.

Visible output should stay structurally stable.

Optional debug addition only under `SEQ_EXPLAIN`:

```txt
SEQ_EXPLAIN_PIPELINE_SOURCE source=actual_pipeline fallback=0
```

or:

```txt
SEQ_EXPLAIN_PIPELINE_SOURCE source=analyzer_recheck fallback=1
```

Do not add this to default `SEQ_TRIAL`.

---

## 11. FieldState handoff

If DetectionRuntime already has `FieldState`, include it in the snapshot.

Analyzer can fill:

```txt
AnalyzerFieldObservation
```

from actual FieldState.

If not readily available, leave:

```txt
field=unknown
```

Do not block Pass I on FieldState.

PatternResult handoff is the priority.

---

## 12. Signal/inspection debug handoff

If the detection pipeline already exposes:

```txt
SignalCandidate
InspectedSignal
inspection reject reason
signal source
```

include them in the snapshot.

If not, skip or leave `hasSignal=false`, `hasInspectedSignal=false`.

Do not recreate a large debug framework in this pass.

---

## 13. Transitional comments

Add a comment near the fallback:

```cpp
// Transitional fallback:
// Analyzer still re-evaluates the candidate only when the actual pipeline
// PatternResult is not available. Pass J compares both paths; Pass K removes
// this fallback once parity is proven.
```

Add a comment near the new handoff:

```cpp
// Actual pipeline handoff:
// DetectionRuntime owns candidate inspection and pattern interpretation.
// Analyzer consumes this result for trial classification.
```

---

## 14. Success criteria

Pass I is successful if:

```txt
Code compiles.
DetectionRuntime exposes or delivers an actual PatternResult snapshot.
AnalyzerReport uses actual pipeline PatternResult when available.
Analyzer-side re-evaluation remains only as fallback.
SEQ_TRIAL format remains unchanged.
SEQ_EXPLAIN format remains unchanged except optional source line.
SEQ_SUMMARY format remains unchanged.
Actual RAW trigger/sample capture is untouched.
No detection thresholds or behavior changed.
```

---

## 15. Quick implementation checklist

```txt
[ ] Find current handleSequenceCandidate handoff.
[ ] Find where DetectionRuntime produces PatternResult.
[ ] Add minimal DetectionPipelineResult or equivalent snapshot.
[ ] Expose latest result or pass it through existing handoff.
[ ] Add source marker: actual_pipeline vs analyzer_recheck.
[ ] Update AnalyzerReport builder to prefer actual PatternResult.
[ ] Keep evaluateRoadmapSignalCandidateImpl fallback.
[ ] Leave SEQ output shapes unchanged.
[ ] Compile.
[ ] Run short SEQ default.
[ ] Run short SEQ explain.
[ ] Confirm actual RAW trigger path untouched.
```

---

## 16. Expected final state of Pass I

After this pass:

```txt
Analyzer can consume actual DetectionRuntime PatternResult.
Analyzer-side re-evaluation still exists as fallback.
```

This prepares Pass J:

```txt
Compare actual pipeline result against Analyzer-side re-check for parity.
```
