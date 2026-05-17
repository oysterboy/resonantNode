# Analyzer / Detection Refactor — Pass L: FrequencyFirst AmpWindow Observation

**Project:** ResonantNode / Resonanzraum  
**Area:** Detection Refactor / FrequencyFirst + AMP evidence  
**Pass:** L  
**Goal:** Add observe-only AMP window evidence to FrequencyFirst candidates so Analyzer can report whether the AMP window makes sense before it is used as an acceptance/rejection gate, with a generic `SEQ_CUSTOM` printer line for the observation.

---

## 0. Context

After Pass K, the normal Analyzer path should consume actual DetectionRuntime `PatternResult` data instead of re-evaluating candidates inside Analyzer.

Next goal:

```txt
Run FrequencyFirst + AmpWindow.
Observe what the AMP window sees.
Decide later whether AMP window can be used to include near/loud events only.
```

This pass is **observation only**.

Do not use AMP window as a hard gate yet.

---

## 1. Core intent

Current desired detection shape:

```txt
FrequencyFirst proposes the candidate.
AmpWindow evaluates retrospective amplitude evidence around that candidate.
PatternResult carries/ exposes the AmpWindow evidence.
Analyzer reports the AmpWindow evidence clearly, either through `SEQ_EXPLAIN` or the generic `SEQ_CUSTOM` line.
PatternRules do not yet reject/accept based on AmpWindow.
```

Target question:

```txt
When FrequencyFirst finds a candidate, what does the AMP window around that candidate look like?
```

Not yet:

```txt
Should this candidate be accepted/rejected based on AMP window?
```

---

## 2. Non-goals

Do not tune thresholds.

Do not add behavior/RB changes.

Do not use AMP window as a gate.

Do not reject far/weak events yet.

Do not change FrequencyFirst acceptance logic except for attaching evidence.

Do not change `SEQ_TRIAL` top-level format unless a compact optional field already exists.

Do not touch actual RAW sample capture.

Do not introduce shared `AudioReporting.h`.

Do not add large persistent buffers or per-trial arrays.

---

## 3. Files to inspect

Detection / frequency path:

```txt
src/detection/DetectionRuntime.*
src/detection/DetectionProfile.h
src/detection/signals/*
src/detection/patterns/*
src/detection/patterns/PatternResult.h
src/audio/*
```

Analyzer reporting:

```txt
src/modes/analyzer/AnalyzerApp.h
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerReporting.h
```

Search for:

```txt
FrequencyFirst
FrequencyMatch
FrequencyMatchDetector
Goertzel
targetFreq
FeatureHistory
ScalarWindow
amp
amplitude
PatternResult
ProfileDetail
SEQ_EXPLAIN_PROFILE_DETAIL
```

---

## 4. Add AmpWindow evidence structure

Add a small, fixed-size evidence struct.

Preferred location:

```txt
detection profile / pattern result detail area
```

or wherever profile-specific evidence currently lives.

Suggested struct:

```cpp
struct AmpWindowEvidence {
    bool available = false;

    int16_t windowStartMs = 0;
    int16_t windowEndMs = 0;

    float peak = 0.0f;
    float baseline = 0.0f;
    float lift = 0.0f;
    float norm = 0.0f;

    const char* supportClass = "unknown";  // none / weak / medium / strong
    const char* localityClass = "unknown"; // far / mid / near / unknown

    bool observedOnly = true;
};
```

If strings are too expensive, use small enums internally and convert to strings only when printing.

Do not allocate arrays.

Do not store raw samples.

---

## 5. Window definition

For each accepted or proposed FrequencyFirst candidate, evaluate an AMP window around candidate time.

Start with conservative defaults:

```txt
amp_window = -20ms .. +120ms relative to frequency candidate onset
```

If current timing makes this unsuitable, choose the closest already used window.

The window should be retrospective/feature-history based, not raw sample capture.

Preferred source:

```txt
FeatureHistory / scalar amplitude stream
```

Fallback only if needed:

```txt
current AMP envelope values already available near candidate
```

Do not create RAW sample dependency.

---

## 6. Evidence fields

Compute and expose:

```txt
amp_peak
amp_baseline
amp_lift = amp_peak - amp_baseline
amp_norm
amp_support_class
locality_class
window start/end
```

Definitions may reuse existing AMP/locality code if available.

If classification bands are not stable yet, use provisional labels:

```txt
support=none|weak|medium|strong|unknown
locality=far|mid|near|unknown
```

But mark them as observation only.

---

## 7. No gating yet

Important:

```txt
FrequencyFirst candidate result must not be rejected because amp_support is weak/none in this pass.
```

Pattern result should remain based on existing FrequencyFirst logic.

AmpWindow evidence is attached as profile detail only.

In code comments:

```cpp
// Observation-only AMP window evidence.
// Do not use this as an acceptance/rejection gate in Pass L.
```

If current PatternRules already use AMP support as gate, add a mode/flag for observe-only or clearly avoid changing that behavior in this pass.

---

## 8. Attach evidence to PatternResult / profile detail

Make the AMP window evidence available to Analyzer.

Preferred:

```txt
PatternResult detail / profile detail contains AmpWindowEvidence.
```

Analyzer should be able to print it in:

```txt
SEQ_EXPLAIN_PROFILE_DETAIL
```

Do not add lots of AMP fields to default `SEQ_TRIAL`.

---

## 9. Analyzer output

Add an explain/detail line.

Preferred line:

```txt
SEQ_CUSTOM trial=17 profile=FrequencyFirst dt=24ms win=-20..120ms peak=71.0 base=42.0 lift=29.0 norm=0.69 support=strong locality=near mode=observe
```

Alternative inside profile detail:

```txt
SEQ_EXPLAIN_PROFILE_DETAIL ns=freq_amp_window freq_score=482000 freq_contrast=1320 amp_win=-20..120ms amp_peak=71.0 amp_base=42.0 amp_lift=29.0 amp_norm=0.69 amp_support=strong locality=near mode=observe
```

Minimum acceptable:

```txt
SEQ_CUSTOM trial=17 win=-20..120ms peak=71.0 base=42.0 lift=29.0 norm=0.69 support=strong locality=near mode=observe
```

Only print in:

```txt
log=explain
```

or explicit profile-detail/debug mode.

Do not spam default `SEQ_TRIAL`.

---

## 10. Optional compact SEQ_TRIAL field

Only if already easy and stable, add one compact field:

```txt
amp_support=strong
```

or:

```txt
amp_locality=near
```

But this is optional.

Preferred for Pass L:

```txt
Keep default SEQ_TRIAL stable and put AMP detail in SEQ_EXPLAIN.
```

---

## 11. Observation runs this should support

The resulting logs should help compare:

```txt
30cm
50cm
70cm
90cm
different mic orientations
different body/cup setups
quiet vs noisy room
```

Questions to answer from logs:

```txt
Do near/loud events produce consistently higher amp_lift?
Do far/quiet events produce lower amp_lift?
Is amp_base stable?
Does the window catch the main hit or mostly tail/reflection?
Does amp_norm separate near/mid/far better than peak?
Are misses frequency misses or AMP evidence failures?
```

---

## 12. Memory constraints

Keep this lean.

Do not store per-trial AMP windows.

Do not store raw samples.

Do not copy FeatureHistory into Analyzer.

Preferred pattern:

```txt
compute AmpWindowEvidence when candidate/pattern is produced
attach compact evidence to PatternResult/profile detail
print immediately in Analyzer explain
update no large buffers
```

This matters especially because RB/runtime should not inherit Analyzer-style heavy diagnostics later.

---

## 13. Success criteria

Pass L is successful if:

```txt
Code compiles.
FrequencyFirst candidates still work.
AMP window evidence is computed for FrequencyFirst candidates.
AMP window evidence is visible in `SEQ_EXPLAIN` or the generic `SEQ_CUSTOM` line.
AMP window is observe-only and does not gate/reject candidates.
SEQ_TRIAL remains stable.
SEQ_SUMMARY remains stable.
Actual RAW sample capture is untouched.
No behavior/RB changes.
No large persistent buffers are added.
```

---

## 14. Quick checklist

```txt
[ ] Find FrequencyFirst/FrequencyMatch candidate path.
[ ] Identify amplitude feature stream/history source.
[ ] Add compact AmpWindowEvidence.
[ ] Evaluate window around candidate time.
[ ] Compute peak/base/lift/norm.
[ ] Add provisional support/locality labels.
[ ] Attach evidence to PatternResult/profile detail.
[ ] Print `SEQ_CUSTOM` or `SEQ_EXPLAIN_PROFILE_DETAIL` line.
[ ] Ensure mode=observe.
[ ] Ensure no acceptance/rejection gate is added.
[ ] Compile.
[ ] Run short SEQ explain test.
[ ] Confirm default SEQ_TRIAL shape unchanged.
[ ] Confirm RAW sample capture untouched.
```

---

## 15. Expected final state

After this pass:

```txt
FrequencyFirst proposes.
AmpWindow observes.
Analyzer reports.
No gating yet.
```

Next possible pass:

```txt
Pass M — AmpWindow band calibration / classification.
Pass N — AmpWindow gating in PatternRules for near/loud inclusion.
```
