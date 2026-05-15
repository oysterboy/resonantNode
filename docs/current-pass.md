# Codex Task: Detection Roadmap v0.3 — Pass 11: Analyzer / Resonant Parity

Use `docs/detection-roadmap-v0-2-implementation-brief.md` as the architecture source of truth, but apply the updated Roadmap v0.3 naming/rule corrections from the latest refactor notes.

## Scope

Analyzer + Resonant detection interpretation parity.

This pass should align Analyzer SEQ interpretation with the same roadmap detection layers used by Resonant Node where appropriate.

Do not refactor behavior.
Do not remove legacy mode.
Do not perform broad cleanup yet.

---

## Goal

Make Analyzer and Resonant Node agree on detection interpretation.

The shared interpretation path should be:

```txt
SignalCandidate
→ SignalInspector
→ InspectedSignal
→ PatternAssembler
→ PatternCandidate
→ PatternRules
→ PatternResult
```

Analyzer may keep Analyzer-specific measurement and SEQ logic, but it should not have a separate private interpretation path that disagrees with Resonant Node.

---

## Why This Pass Exists

After Pass 10, `RoadmapFrequencyFirst` is the default RB path and runs better than old AMP.

Now Analyzer should be aligned so SEQ results describe the same detection semantics that RB uses.

Analyzer remains the measurement tool, but it should share the interpretation layers.

---

## Important Boundary

Analyzer-specific logic may stay in Analyzer:

```txt
trial scheduling
trigger timing
warmup / test windows
expected / late / miss classification
distance labels
quiet test handling
summary reporting
raw diagnostic logging
```

Shared detection interpretation should move/use:

```txt
SignalInspector
PatternAssembler
PatternRules
```

Do not make Analyzer identical to Node.

Make Analyzer agree with Node on what counts as:

```txt
valid frequency signal
accepted inspected signal
behavior-eligible PatternResult
transient-only fallback
frequency-primary result
invalid / rejected result
```

---

## Current Known Issue

Older Analyzer report lines may still show stale legacy labels such as:

```txt
source=amp_fallback
reject=freq_score_too_low
```

even when the newer frequency candidate path says:

```txt
source=frequency_primary
candidate_reject=none
proposer_matched=1
```

This pass should fix that semantic mismatch.

SEQ output should not report a good roadmap frequency candidate as `amp_fallback` just because an old compatibility/classification helper was used.

---

## Required Work

### 1. Identify Analyzer interpretation duplicates

Find places where Analyzer directly performs old interpretation such as:

```txt
FrequencyEvidenceEvaluation::classifyPatternResult(...)
DetectionPipeline::processDetectorCandidate(...)
manual candidate_class assignment
manual source=amp_fallback / comparison_only assignment
manual freq reject assignment
```

Do not remove everything blindly.

Mark which parts are:

```txt
Analyzer measurement/reporting only
```

and which parts are:

```txt
detection interpretation that should use shared layers
```

---

### 2. Route Analyzer roadmap candidate interpretation through shared layers

Where Analyzer has a roadmap frequency candidate / SignalCandidate, use the same path as Resonant Node:

```txt
SignalInspector
→ PatternAssembler
→ PatternRules
```

At minimum, for frequency proposer candidates:

```txt
Frequency SignalCandidate
→ SignalInspector
→ InspectedSignal
→ PatternAssembler
→ PatternRules
→ PatternResult
```

Then use the resulting `PatternResult` for:

```txt
candidate_class
source
tonalValid
behaviorEligible
rejectReason
PatternType
```

---

### 3. Preserve SEQ trial classification

Do not break SEQ trial result semantics:

```txt
expected
late
miss
unexpected
duplicate
invalid_audio
```

These are measurement outcomes and may remain Analyzer-owned.

But the candidate interpretation inside a trial should use shared PatternResult semantics.

---

### 4. Fix stale report labels

SEQ report lines should reflect roadmap interpretation.

If a frequency proposer candidate created the primary result, output should say something like:

```txt
source=frequency_primary
candidate_class=expected_primary
reject=none
```

not:

```txt
source=amp_fallback
reject=freq_score_too_low
```

when the roadmap PatternResult is actually valid and behavior-eligible.

---

### 5. Keep proposer vs window evidence distinct

Do not reintroduce ambiguity.

Use names like:

```txt
proposer_score
proposer_contrast
windowEarly_score
windowEarly_contrast
windowFull_score
windowFull_contrast
```

If window evidence is not independently measured, print:

```txt
window_present=0
window_reason=not_measured
```

or keep current behavior if already fixed.

Do not copy proposer evidence into window fields and present it as independent evidence.

---

### 6. Keep real AMP separate from compatibility/source candidate

Do not label a frequency-derived compatibility candidate as real AMP.

If candidate source is frequency, print:

```txt
sourceCand[present=1 source=frequency ...]
```

If a real AMP candidate exists, print:

```txt
ampCand[present=1 ...]
```

If not:

```txt
ampCand[present=0]
```

Do not report frequency score as AMP strength unless clearly labeled as compatibility mapping.

---

## Suggested Helper

If useful, add a small Analyzer helper that uses shared layers:

```cpp
bool AnalyzerApp::evaluateRoadmapSignalCandidate(
    const detection::SignalCandidate& signal,
    DetectionPipeline::PatternResult& outResult
);
```

Internally:

```txt
SignalInspector.inspect(...)
PatternAssembler.acceptSignal(...)
PatternAssembler.popPatternCandidate(...)
PatternRules.evaluate(...)
```

Keep it small.

Do not instantiate a full `DetectionRuntime` inside Analyzer unless that is already simple and safe.

For this pass, sharing the interpretation layers is enough.

---

## Do Not

- do not change ResonantBehavior
- do not change behavior timing
- do not tune thresholds
- do not remove AmpLegacy mode
- do not remove old Analyzer diagnostics
- do not rewrite Analyzer entirely
- do not remove SEQ measurement logic
- do not change output/chirp behavior
- do not add DetectionStrategy/Profile
- do not add FieldState
- do not implement complex pattern grouping
- do not implement overlap dominance
- do not perform broad file/class cleanup
- do not delete old classes broadly

---

## Acceptance Criteria

- Project compiles.
- Analyzer frequency proposer candidates are interpreted through shared roadmap layers where appropriate.
- Analyzer and RB agree on frequency-primary PatternResult semantics.
- SEQ reports no longer label valid roadmap frequency-primary results as stale `amp_fallback` / `freq_score_too_low`.
- SEQ trial classification still works: expected / late / miss / duplicate / unexpected.
- Analyzer-specific measurement logic remains intact.
- Proposer and window evidence remain clearly distinct.
- Real AMP and frequency source candidates remain clearly distinct.
- No behavior/runtime output logic is changed.
- Thresholds are unchanged.

---

## Post-Pass Test Plan

Run Analyzer SEQ:

```txt
SEQ 70cm
SEQ 140cm
SEQ quiet
```

Check:

```txt
SEQ_TRIAL and SEQ_REPORT agree on source/reject semantics
valid frequency proposer candidates report source=frequency_primary
valid frequency proposer candidates do not report reject=freq_score_too_low
misses still report as misses
quiet false positives are visible
duplicates remain visible
```

Then run RB smoke test:

```txt
RB DETECT
RB detectonly on
RB log full
```

Confirm:

```txt
default mode is RoadmapFrequencyFirst
RB still emits behavior-eligible PatternResults
legacy mode still available
```

---

## Notes for Later Cleanup

Do not clean old classes in this pass.

Cleanup remains later:

```txt
Pass 13 — Remove / isolate legacy path
Pass 14 — Naming / file cleanup
```
