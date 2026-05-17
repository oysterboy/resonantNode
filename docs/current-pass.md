# Analyzer Refactor — Pass B: AnalyzerReporting Skeleton

**Project:** ResonantNode / Resonanzraum  
**Area:** Detection Refactor / Analyzer  
**Pass:** B  
**Goal:** Introduce the stable Analyzer reporting vocabulary and minimal report structs without changing detection behavior or current output behavior.

Status: Pass B is complete. The next active pass is Pass C.

---

## 0. Context

Pass A quarantines legacy Analyzer/SEQ output and introduces the preferred `SEQ_EXPLAIN` naming.

Pass B adds the new Analyzer reporting model that later passes will use for:

```txt
SEQ_TRIAL
SEQ_EXPLAIN
SEQ_SUMMARY
profile-comparable reporting
legacy output removal
```

This pass should be structural only.

Do not rewrite trial classification yet.

Do not replace existing output yet.

---

## 1. Core intent

Add a small, stable Analyzer reporting layer.

The Analyzer should start moving toward this conceptual shape:

```txt
RunContext
ExpectedEvent
PatternObservation
SignalObservation
InspectionObservation
FieldObservation
AnalyzerClassification
ProfileDetail
DebugSummary
AnalyzerReport
```

But Pass B only needs a minimal useful skeleton.

---

## 2. Files to add

Add:

```txt
src/modes/analyzer/AnalyzerReporting.h
```

Optional only if needed:

```txt
src/modes/analyzer/AnalyzerReporting.cpp
```

Prefer header-only name helpers if that matches the current code style.

Do not move large Analyzer logic into this file yet.

Final file plan:

```txt
src/modes/analyzer/AnalyzerReporting.h     now
src/detection/AudioReporting.h             later shared layer
src/behavior/BehaviorReporting.h           later RB layer
```

---

## 3. Files to inspect

Inspect:

```txt
src/modes/analyzer/AnalyzerApp.h
src/modes/analyzer/AnalyzerApp.cpp
```

Also inspect current detection result structs if needed:

```txt
src/detection/patterns/PatternResult.h
src/detection/signals/SignalCandidate.h
src/detection/signals/InspectedSignal.h
src/detection/field/FieldState.h
```

But do not refactor those files in this pass unless required for include correctness.

---

## 4. Add AnalyzerResult

Add:

```cpp
enum class AnalyzerResult {
    Expected,
    Early,
    Late,
    Miss,
    Duplicate,
    Unexpected,
    Rejected,
    Ambiguous,
    TooDense,
    InvalidAudio,
    Unknown
};
```

Notes:

```txt
Expected     = valid PatternResult inside expected window
Early        = valid PatternResult before expected window
Late         = valid PatternResult after expected window
Miss         = no usable signal/pattern for expected event
Duplicate    = duplicate candidate/pattern after primary
Unexpected   = valid PatternResult without expected trigger
Rejected     = signal/pattern candidate existed but was rejected
Ambiguous    = multiple competing interpretations
TooDense     = field too dense/noisy for clean classification
InvalidAudio = audio overflow/invalid input path
Unknown      = temporary fallback during migration
```

`Unknown` is useful during migration.

---

## 5. Add AnalyzerReason

Add:

```cpp
enum class AnalyzerReason {
    None,
    ValidPatternInExpectedWindow,
    ValidPatternBeforeWindow,
    ValidPatternAfterWindow,
    NoSignalCandidate,
    SignalSeenButRejected,
    InspectionFailed,
    PatternCandidateRejected,
    MultipleValidPatterns,
    MultipleCompetingPatterns,
    FieldTooDense,
    UnexpectedValidPatternWithoutTrigger,
    DuplicatePatternAfterPrimary,
    InvalidAudio,
    Unknown
};
```

Notes:

```txt
None    = no reason assigned yet
Unknown = temporary migration fallback
```

Keep reason vocabulary stable and explicit.

Do not encode profile-specific detail in `AnalyzerReason`.

Profile-specific facts belong later in `ProfileDetail`.

---

## 6. Add name helpers

Add helpers:

```cpp
const char* analyzerResultName(AnalyzerResult value);
const char* analyzerReasonName(AnalyzerReason value);
```

Return lowercase, log-safe names:

```txt
expected
early
late
miss
duplicate
unexpected
rejected
ambiguous
too_dense
invalid_audio
unknown
```

and:

```txt
none
valid_pattern_in_expected_window
valid_pattern_before_window
valid_pattern_after_window
no_signal_candidate
signal_seen_but_rejected
inspection_failed
pattern_candidate_rejected
multiple_valid_patterns
multiple_competing_patterns
field_too_dense
unexpected_valid_pattern_without_trigger
duplicate_pattern_after_primary
invalid_audio
unknown
```

These strings should become the canonical log names.

---

## 7. Add AnalyzerRunContext

Add a minimal context struct:

```cpp
struct AnalyzerRunContext {
    const char* profileName = "unknown";
    unsigned long trial = 0;
    unsigned long nowMs = 0;

    const char* mode = "SEQ";
    const char* expectedPattern = "unknown";

    unsigned long targetHz = 0;
    long expectedWindowStartMs = -1;
    long expectedWindowEndMs = -1;
};
```

Notes:

```txt
profileName is mandatory later, but "unknown" is acceptable during migration.
trial should be filled for SEQ.
targetHz is useful for FreqAmp/Frequency profiles.
expected window is signed so -1 can mean unset.
```

---

## 8. Add PatternObservation

Add a generic Analyzer-side view of the primary PatternResult:

```cpp
struct AnalyzerPatternObservation {
    const char* type = "none";
    bool accepted = false;

    float confidence = 0.0f;
    long dtMs = -1;

    const char* locality = "unknown";
    const char* sourceClass = "unknown";
    const char* reason = "none";

    unsigned int involvedSignals = 0;
};
```

Notes:

```txt
This is not a new PatternResult implementation.
It is an Analyzer report view over whichever PatternResult the selected DetectionProfile produced.
```

Do not hardcode FreqAmp-only fields here.

---

## 9. Add SignalObservation

Add a compact signal summary:

```cpp
struct AnalyzerSignalObservation {
    unsigned int total = 0;
    unsigned int accepted = 0;
    unsigned int rejected = 0;

    const char* primarySource = "none";
    long primaryDtMs = -1;
    unsigned long primaryDurationMs = 0;
    float primaryStrength = 0.0f;
    float primaryConfidence = 0.0f;

    const char* mainRejectReason = "none";
    bool duplicateRisk = false;
};
```

Notes:

```txt
This is for SEQ_EXPLAIN and debugging.
It should not dominate default SEQ_TRIAL.
```

---

## 10. Add InspectionObservation

Add a generic inspection summary:

```cpp
struct AnalyzerInspectionObservation {
    unsigned int inspected = 0;
    unsigned int accepted = 0;
    unsigned int rejected = 0;

    const char* primaryEvidence = "none";
    const char* locality = "unknown";
    const char* supportClass = "unknown";
    const char* mainRejectReason = "none";
};
```

Notes:

```txt
This is intentionally generic.
Detailed freq/amp values will go into ProfileDetail in later passes.
```

---

## 11. Add FieldObservation

Add:

```cpp
struct AnalyzerFieldObservation {
    const char* state = "unknown";
    float activity = 0.0f;
    float density = 0.0f;

    unsigned int recentValidPatterns = 0;
    unsigned int recentRejects = 0;
};
```

Notes:

```txt
FieldState may not be available yet in current Analyzer code.
Keep default "unknown" acceptable.
```

---

## 12. Add AnalyzerClassification

Add:

```cpp
struct AnalyzerClassification {
    AnalyzerResult result = AnalyzerResult::Unknown;
    AnalyzerReason reason = AnalyzerReason::Unknown;

    long dtMs = -1;
    float confidence = 0.0f;
};
```

This is the stable classification layer used later by `SEQ_TRIAL` and `SEQ_SUMMARY`.

---

## 13. Add ProfileDetail placeholder

Add a small placeholder for profile-specific detail.

Keep it simple.

Option A — string-only summary:

```cpp
struct AnalyzerProfileDetail {
    const char* namespaceName = "none";
    const char* summary = "";
};
```

Option B — simple common detail fields:

```cpp
struct AnalyzerProfileDetail {
    const char* namespaceName = "none";

    float freqScore = 0.0f;
    float freqContrast = 0.0f;

    float ampLevel = 0.0f;
    float ampBase = 0.0f;
    float ampLift = 0.0f;
    float ampNorm = 0.0f;

    const char* ampLocality = "unknown";
};
```

Prefer Option A if avoiding profile-specific fields in the skeleton.

Prefer Option B if current Analyzer already prints these values everywhere and it helps the next pass.

If unsure, choose Option A now and add detailed namespaces in Pass E.

---

## 14. Add DebugSummary

Add:

```cpp
struct AnalyzerDebugSummary {
    unsigned int signals = 0;
    unsigned int inspected = 0;
    unsigned int patterns = 0;

    unsigned int rejects = 0;
    unsigned int duplicates = 0;
    unsigned int unexpected = 0;

    const char* mainRejectReason = "none";
};
```

This supports later `SEQ_EXPLAIN` and `SEQ_SUMMARY`.

---

## 15. Add AnalyzerReport

Add:

```cpp
struct AnalyzerReport {
    AnalyzerRunContext context;

    AnalyzerPatternObservation primaryPattern;
    AnalyzerSignalObservation signals;
    AnalyzerInspectionObservation inspection;
    AnalyzerFieldObservation field;

    AnalyzerClassification classification;
    AnalyzerProfileDetail profileDetail;
    AnalyzerDebugSummary debug;
};
```

Keep this as a plain data struct.

Do not add heavy behavior.

Do not add dependencies on `AnalyzerApp`.

---

## 16. Include integration

In `AnalyzerApp.h` or `AnalyzerApp.cpp`, include:

```cpp
#include "modes/analyzer/AnalyzerReporting.h"
```

or relative include according to current project style.

At the end of Pass B, it is acceptable if `AnalyzerReport` is not yet used.

However, prefer adding a small compile-time sanity usage if warnings require it.

Example:

```cpp
// No runtime use yet. Pass C will build AnalyzerReport from finalized trials.
```

---

## 17. Optional helper: empty report factory

Optional:

```cpp
inline AnalyzerReport makeEmptyAnalyzerReport() {
    return AnalyzerReport{};
}
```

Only add this if useful.

Do not add builder logic in Pass B.

Builder logic belongs to Pass C.

---

## 18. Non-goals

Do not implement these in Pass B:

```txt
new SEQ_TRIAL output
SEQ_EXPLAIN formatting
SEQ_SUMMARY rebuild
classification rewrite
PatternResult mapping
profile switching
legacy output removal
TrialReport removal
AudioReporting extraction
BehaviorReporting
RAW sample capture changes
```

---

## 19. Success criteria

Pass B is successful if:

```txt
Code compiles.
AnalyzerReporting.h exists.
AnalyzerResult and AnalyzerReason exist.
Canonical name helpers exist.
AnalyzerReport skeleton exists.
AnalyzerApp can include the new header.
No detection behavior changes.
No SEQ output behavior changes required.
No RAW trigger changes.
```

---

## 20. Recommended quick checks

After implementation:

```txt
[x] Build / compile.
[x] Confirm AnalyzerApp includes the new header without circular include problems.
[x] Confirm old SEQ command still compiles.
[x] Confirm actual RAW trigger code was not modified.
[x] Grep for AnalyzerResult / AnalyzerReason to confirm they only appear in new skeleton unless small compile integration was needed.
```

---

## 21. Expected final state of Pass B

The codebase now has a stable target structure for Analyzer reporting:

```txt
AnalyzerResult
AnalyzerReason
AnalyzerReport
```

But old output and old classification still run as before.

This prepares Pass C:

```txt
Build AnalyzerReport from current finalized trial data.
```
