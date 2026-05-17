# Analyzer Refactor — Pass H: Profile Switching Hardening

**Project:** ResonantNode / Resonanzraum  
**Area:** Detection Refactor / Analyzer  
**Pass:** H  
**Goal:** Ensure Analyzer reporting remains stable when DetectionProfiles become switchable and different profiles emit different PatternResult details.
Status: Pass H is verified on-device. The Analyzer now accepts `SEQ profile=freqamp|ampstate|chirp` and keeps the report shape stable across profile switches.

---

## 0. Context

Previous passes:

```txt
Pass A — Legacy output quarantine
Pass B — AnalyzerReporting skeleton
Pass C — Build AnalyzerReport from current trial
Pass D — New compact default SEQ_TRIAL
Pass E — SEQ_EXPLAIN
Pass F — SEQ_SUMMARY cleanup
Pass G — Legacy report storage separation
```

After Pass G:

```txt
AnalyzerReport = normal reporting path
SequenceTest::TrialReport = legacy-only path
```

Pass H makes the report path profile-safe.

---

## 1. Core intent

The Analyzer should not break or change top-level output format when switching profiles.

Stable across profiles:

```txt
profile
trial
result
reason
pattern
dt
confidence
locality
source
field
duplicates
candidates
summary counts
```

Variable per profile:

```txt
feature streams
signal detectors
inspection evidence
pattern vocabulary
confidence calculation
profile detail fields
```

The Analyzer should read profile-specific results through a generic view.

---

## 2. Non-goals

Do not implement a full DetectionProfile switching UI if it does not exist yet.

Do not change detection behavior.

Do not change thresholds.

Do not refactor Runtime Behavior.

Do not introduce shared `AudioReporting.h` unless there is already duplication with RB reporting.

Do not remove all legacy code unless Pass G made it trivial and safe.

Do not touch actual RAW sample capture.

---

## 3. Files to inspect

Start with:

```txt
src/modes/analyzer/AnalyzerApp.h
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerReporting.h
```

Detection profile/result files:

```txt
src/detection/DetectionProfile.h
src/detection/DetectionRuntime.*
src/detection/patterns/PatternResult.h
src/detection/patterns/*
src/detection/signals/*
src/detection/field/FieldState.h
```

Search for:

```txt
profile
DetectionProfile
PatternResult
PatternKind
PatternType
locality
source
ampSupport
reasonCode
rejectReason
confidence
```

---

## 4. Active profile name

Ensure every Analyzer report includes a stable profile name:

```txt
profile=FreqAmp
profile=AmpOnly
profile=ChirpField
profile=FrequencyFirst
profile=unknown
```

The helper from Pass C should now be hardened:

```cpp
const char* AnalyzerApp::activeAnalyzerProfileName() const;
```

It should use the actual selected profile when available.

If real switching is not implemented yet, keep a clear default:

```txt
FreqAmp
```

Do not leave normal output as:

```txt
profile=unknown
```

unless truly unavoidable.

---

## 5. Generic PatternResult view

Ensure `AnalyzerPatternObservation` is filled through a generic mapping helper.

Suggested helper:

```cpp
AnalyzerPatternObservation makeAnalyzerPatternObservation(
    const detection::PatternResult& pattern,
    long dtMs
);
```

or:

```cpp
void fillAnalyzerPatternObservation(
    AnalyzerPatternObservation& out,
    const detection::PatternResult& pattern,
    long dtMs
);
```

This helper should be the only place where Analyzer knows about `PatternResult` internals.

---

## 6. Required generic pattern fields

The mapping should support:

```txt
type
accepted
confidence
dtMs
locality
sourceClass
reason
involvedSignals
```

These fields must remain available regardless of profile.

If a profile does not provide a concept:

```txt
locality=unknown
source=unknown
confidence=0.00
```

Do not omit the field.

---

## 7. Pattern type strategy

Different profiles may emit different pattern types:

```txt
neighbor_chirp
far_chirp
amp_transient
frequency_match
broadband_hit
woodblock_hit
noise_burst
none
unknown
```

Analyzer should not hardcode behavior for each type.

It should print:

```txt
pattern=<generic type string>
```

Avoid `switch` statements over all possible future profile pattern types unless required for string conversion.

Prefer `PatternResult` exposing or mapping to a stable string name.

---

## 8. Reason strategy

Analyzer classification reason remains Analyzer-specific:

```txt
AnalyzerReason
```

Pattern-specific reason remains profile/pattern-specific:

```txt
report.primaryPattern.reason
```

Do not confuse them.

Example:

```txt
SEQ_TRIAL ... result=miss reason=no_signal_candidate pattern=none ...
```

versus:

```txt
SEQ_EXPLAIN_PATTERN ... reason=insufficient_locality
SEQ_EXPLAIN_CLASSIFICATION ... reason=signal_seen_but_rejected
```

Top-level `reason=` in `SEQ_TRIAL` should mean `AnalyzerReason`.

Profile/pattern reason belongs in `SEQ_EXPLAIN_PATTERN` or profile detail.

---

## 9. Profile detail namespaces

Add or harden optional namespaces:

```txt
detail.freq.*
detail.amp.*
detail.chirp.*
detail.noise.*
```

In code this can be simple at first.

Examples:

```txt
SEQ_EXPLAIN_PROFILE_DETAIL ns=freq_amp freq_score=482000 freq_contrast=1320 amp_locality=near
```

or minimal:

```txt
SEQ_EXPLAIN_PROFILE_DETAIL ns=freq_amp summary=""
```

Top-level `SEQ_TRIAL` should not depend on these details.

---

## 10. Keep top-level SEQ_TRIAL profile-neutral

`SEQ_TRIAL` should not include required fields like:

```txt
freqEarly
freqFull
ampLift
ampNorm
goertzelScore
chirpGap
noiseBand
```

These belong in:

```txt
SEQ_EXPLAIN_PROFILE_DETAIL
SEQ_PROFILE_SUMMARY
legacy/explain detail
```

Top-level must stay:

```txt
trial profile result pattern dt confidence locality source field reason dup candidates
```

---

## 11. Summary profile comparability

Verify `SEQ_SUMMARY` uses the same fields regardless of profile:

```txt
SEQ_SUMMARY profile=<profile> trials=... expected=... early=... late=... miss=... duplicate=... unexpected=... rejected=... ambiguous=... too_dense=... invalid_audio=...
```

Profile-specific summary should be separate:

```txt
SEQ_PROFILE_SUMMARY profile=FreqAmp ...
```

Do not add profile-specific counters to the main `SEQ_SUMMARY`.

---

## 12. FieldState stability

If FieldState is available:

```txt
field=quiet|active|busy|dense|unknown
```

If profiles do not use FieldState yet, keep:

```txt
field=unknown
```

Do not make Analyzer depend on a profile-specific field-state implementation.

---

## 13. Optional ProfileDetail improvement

If current `AnalyzerProfileDetail` is too minimal, add a small stable structure:

```cpp
struct AnalyzerProfileDetail {
    const char* namespaceName = "none";
    const char* summary = "";

    float freqScore = 0.0f;
    float freqContrast = 0.0f;

    float ampLevel = 0.0f;
    float ampBase = 0.0f;
    float ampLift = 0.0f;
    float ampNorm = 0.0f;

    const char* ampLocality = "unknown";
};
```

But only add fields that are already useful and cheap.

Do not turn `AnalyzerProfileDetail` into a dumping ground.

---

## 14. Avoid premature shared reporting

Do not extract `AudioReporting.h` in this pass unless there is already a clear second consumer.

Keep note:

```txt
AnalyzerReporting.h stays Analyzer-specific.
AudioReporting.h may later hold shared PatternReportView / FieldReportView.
BehaviorReporting.h may later hold RB decision/action reports.
```

Shared later:

```txt
profile name
PatternReportView
FieldReportView
timing / confidence / locality / source vocabulary
```

Not shared:

```txt
AnalyzerResult
AnalyzerReason
expected-window classification
BehaviorDecision
BehaviorAction
suppression / refractory / probability logic
```

---

## 15. Compatibility with future profiles

Check that adding a new profile would require changes only in:

```txt
profile selection
PatternResult production
PatternResult → AnalyzerPatternObservation mapping if new generic fields are needed
optional profile detail mapping
```

It should not require changing:

```txt
SEQ_TRIAL format
SEQ_SUMMARY format
AnalyzerResult vocabulary
AnalyzerReason vocabulary
default print functions
```

---

## 16. Cleanup of profile-specific top-level fields

If any profile-specific details still appear in top-level default output, move them to:

```txt
SEQ_EXPLAIN_PROFILE_DETAIL
SEQ_PROFILE_SUMMARY
legacy detail
```

Examples to remove from top-level:

```txt
freqEarly
freqFull
amp_level
amp_base
amp_lift
amp_norm
freqCompare
proposerCand
ampCand
```

---

## 17. Success criteria

Pass H is successful if:

```txt
Code compiles.
SEQ_TRIAL format remains unchanged across profiles.
SEQ_SUMMARY format remains unchanged across profiles.
Every report includes a meaningful profile name.
Analyzer reads PatternResult through a generic mapping helper.
Profile-specific detail is optional and namespaced.
Top-level output does not require FreqAmp-specific fields.
Legacy output remains quarantined.
Actual RAW trigger/sample capture is untouched.
No detection thresholds or behavior changed.
```

---

## 18. Quick implementation checklist

```txt
[x] Harden activeAnalyzerProfileName().
[x] Centralize PatternResult → AnalyzerPatternObservation mapping.
[x] Ensure missing profile concepts print as unknown/none, not omitted.
[x] Keep AnalyzerReason separate from pattern-specific reason.
[x] Ensure SEQ_TRIAL has no required profile-specific fields.
[x] Ensure SEQ_SUMMARY has no required profile-specific fields.
[x] Move profile-specific detail to SEQ_EXPLAIN_PROFILE_DETAIL or SEQ_PROFILE_SUMMARY.
[x] Add comments about future AudioReporting extraction.
[x] Compile.
[x] Run SEQ with current profile.
[x] If profile switching exists, run at least two profiles and compare line shape.
[x] Confirm RAW trigger path untouched.
```

---

## 19. Expected final state of Pass H

After this pass:

```txt
Analyzer output is stable across DetectionProfiles.
```

Final Analyzer reporting model:

```txt
SEQ_TRIAL = compact truth
SEQ_EXPLAIN = why/how
SEQ_SUMMARY = run comparison
Profile detail = optional and namespaced
RAW_SAMPLE_CAPTURE = separate diagnostic command
Legacy output = quarantined and removable
```

This prepares the later optional pass:

```txt
AudioReporting extraction, only if Analyzer and Runtime Behavior actually need shared report views.
```
