# Pass 6 — Classifier Integration Refactor Plan

## Goal

Move frequency validation from logging-only diagnostics into the shared classification layer.

The frequency knobs already exist:

- `freqScore = 50000`
- `freqContrast = 20.0`

Defined in:

- `src/detection/FrequencyEvidenceEvaluation.h`

Accepted in:

- `AnalyzerApp.cpp`
- `node.cpp`

Status:

- `PatternResult` classification already uses `freqScore` and `freqContrast`.
- Analyzer now tracks classifier-level counts separately from the old AMP counters.
- ResonantBehavior can require tonal validity at runtime with `RB BEHAV requireTonal=1`.
- Both `esp32dev` and `esp32dev-analyzer` compile cleanly with the current changes.

---

## Core Rule

Do not integrate frequency validation into the low-level AMP/transient detector.

Keep this separation:

```text
AMP / transient detector
→ DetectorCandidate
→ FrequencyEvidence
→ PatternClassifier
→ PatternResult
→ Analyzer / ResonantBehavior
```

The detector answers:

```text
Was there a transient candidate?
```

The classifier answers:

```text
What kind of candidate is this?
```

---

## Non-Goals

Do not:

- tune AMP thresholds
- change onset / release thresholds
- change cooldown
- change min/max transient duration
- change `minStrength`
- change the frequency probe algorithm
- implement FFT
- implement full chirp grouping
- implement family matching
- implement overlap dominance
- remove transient-first path
- hide rejected/weak candidates from logs

---

## Desired Result

A candidate should no longer be only:

```text
accepted transient
```

It should become something like:

```text
candidateValid = true
tonalValid = true
behaviorEligible = true
patternType = valid_tonal_chirp
reason = freq_score_and_contrast_ok
```

or:

```text
candidateValid = true
tonalValid = false
behaviorEligible = false
patternType = transient_only
reason = freq_score_and_contrast_too_low
```

Important distinction:

```text
candidate rejected
```

is not the same as:

```text
candidate exists but is not tonal-valid
```

Duplicates and residual acoustic activity should remain visible in logs.

---

## Step 1 — Centralize Frequency Threshold Evaluation DONE

Add one central place where frequency evidence is evaluated against tuning.

Avoid duplicating this logic in multiple log callsites.
Avoid duplicating it  for Analyser and node/Resonantbehavior 

Suggested struct:

```cpp
struct FrequencyEvidence {
    bool present = false;
    bool validWindow = false;

    float score = 0.0f;
    float contrast = 0.0f;

    float targetHz = 0.0f;
    float targetPower = 0.0f;
    float neighborPower = 0.0f;
    float totalEnergy = 0.0f;

    uint32_t observedAtMs = 0;
    uint32_t ageMs = 0;

    bool scoreOk = false;
    bool contrastOk = false;
    bool matched = false;

    PatternRejectReason reason = PatternRejectReason::None;
};
```

Evaluation rule:

```cpp
scoreOk = score >= tuning.freqScore;
contrastOk = contrast >= tuning.freqContrast;
matched = present && validWindow && scoreOk && contrastOk;
```

Reason logic:

```cpp
if (!present) reason = NoFrequencyEvidence;
else if (!validWindow) reason = FrequencyWindowInvalid;
else if (!scoreOk && !contrastOk) reason = FrequencyScoreAndContrastTooLow;
else if (!scoreOk) reason = FrequencyScoreTooLow;
else if (!contrastOk) reason = FrequencyContrastTooLow;
else reason = None;
```

---

## Step 2 — Add Explicit Pattern Reasons DONE

Use enum internally.  
Stringify only for logs.

Suggested enum:

```cpp
enum class PatternRejectReason {
    None,

    NoCandidate,

    NoFrequencyEvidence,
    FrequencyWindowInvalid,
    FrequencyScoreTooLow,
    FrequencyContrastTooLow,
    FrequencyScoreAndContrastTooLow,

    TransientOnly,
    DuplicateAfterPrimary,
    UnexpectedTiming,
    UnexpectedNoise
};
```

Suggested log strings:

```text
none
no_candidate
no_frequency_evidence
frequency_window_invalid
freq_score_too_low
freq_contrast_too_low
freq_score_and_contrast_too_low
transient_only
duplicate_after_primary
unexpected_timing
unexpected_noise
```

---

## Step 3 — Extend PatternResult DONE

PatternResult should expose separate facts:

```cpp
struct PatternResult {
    bool candidateValid = false;      // AMP/transient candidate exists
    bool tonalValid = false;          // frequency evidence passed thresholds
    bool behaviorEligible = false;    // allowed to trigger behavior

    PatternType type = PatternType::None;
    PatternRejectReason reason = PatternRejectReason::None;

    DetectorCandidate candidate;
    FrequencyEvidence freq;
};
```

Suggested PatternType:

```cpp
enum class PatternType {
    None,

    ValidTransient,
    ValidTonalChirp,

    TransientOnly,
    FrequencyWeak,

    DuplicateAfterPrimary,
    UnexpectedNoise
};
```

Classification rule:

```text
candidateValid = accepted AMP/transient candidate
tonalValid = frequency evidence matched thresholds
behaviorEligible = candidateValid && tonalValid
```

For now, avoid making `patternValid` ambiguous.  
Prefer explicit flags.

---

## Step 4 — Classifier Logic DONE

Pseudo-flow:

```cpp
PatternResult classifyCandidate(
    const DetectorCandidate& candidate,
    const FrequencyEvidence& freq,
    const ClassifierTuning& tuning
) {
    PatternResult result;

    result.candidate = candidate;
    result.freq = evaluateFrequency(freq, tuning);

    result.candidateValid = candidate.accepted;

    if (!result.candidateValid) {
        result.type = PatternType::None;
        result.reason = PatternRejectReason::NoCandidate;
        return result;
    }

    result.tonalValid = result.freq.matched;

    if (result.tonalValid) {
        result.type = PatternType::ValidTonalChirp;
        result.reason = PatternRejectReason::None;
        result.behaviorEligible = true;
    } else {
        result.type = PatternType::TransientOnly;
        result.reason = result.freq.reason;
        result.behaviorEligible = false;
    }

    return result;
}
```

Important: this should happen after candidate extraction, not inside the AMP detector.

---

## Step 5 — Fix Frequency Logging Semantics DONE

Current problem:

```text
freq_present=1
freq_matched=0
freq_conf=0.0
freq_score=700000
freq_contrast=1200
```

Status:

- Implemented in `AnalyzerApp.cpp` and `node.cpp`.
- The candidate headline logs now print the classified `patternResult.freq` snapshot, so `freq_matched`, `freq_conf`, and related fields line up with the classifier result instead of the raw detector-only struct.
- Both `esp32dev` and `esp32dev-analyzer` compile cleanly with this change.

After this pass, strong primary candidates should log:

```text
freq_present=1
freq_valid_window=1
freq_score=700000
freq_contrast=1200
freq_score_ok=1
freq_contrast_ok=1
freq_matched=1
pattern_type=valid_tonal_chirp
behavior_eligible=1
```

Weak duplicates should log:

```text
freq_present=1
freq_valid_window=1
freq_score=0.3
freq_contrast=1.6
freq_score_ok=0
freq_contrast_ok=0
freq_matched=0
pattern_type=transient_only
pattern_reason=freq_score_and_contrast_too_low
behavior_eligible=0
```

---

## Step 6 — Analyzer Integration DONE - NEEDS TESTING

Analyzer / SEQ should continue to preserve old AMP-based metrics:

```text
expected_hits
late_hits
misses
unexpected
duplicates
valid_primary
```

Add classifier-level counters separately:

```text
tonal_expected
transient_only_expected
tonal_duplicates
non_tonal_duplicates
tonal_unexpected
non_tonal_unexpected

freq_reject_score
freq_reject_contrast
freq_reject_both
freq_reject_no_evidence
freq_reject_invalid_window
```

Suggested output:

```text
SEQ_SUMMARY ...
SEQ_CLASS_SUMMARY tonal_expected=...
                  transient_only_expected=...
                  tonal_duplicates=...
                  non_tonal_duplicates=...
                  freq_reject_score=...
                  freq_reject_contrast=...
                  freq_reject_both=...
```

This keeps old test results comparable while adding classifier validation.

---

## Step 7 — ResonantBehavior Integration

Do not immediately make behavior depend permanently on tonal validity.

Add a switch:

```cpp
bool requireTonalForBehavior = false;
```

or runtime param:

```text
requireTonal=0/1
```

Default for first test:

```text
requireTonal=0
```

Status:

- `RB BEHAV requireTonal=0/1` is implemented.
- When `requireTonal=1`, blocked candidates log `RB_BLOCK reason=...` with the classifier reject reason.

Expected behavior:

```text
requireTonal=0:
  RB behaves as before, but logs tonal classification.

requireTonal=1:
  RB only reacts to candidates with behaviorEligible=true.
```

Blocked candidates should log explicit reasons:

```text
RB_BLOCK reason=freq_score_too_low
RB_BLOCK reason=freq_contrast_too_low
RB_BLOCK reason=freq_score_and_contrast_too_low
RB_BLOCK reason=transient_only
```

---

## Step 8 — Test Order

### 1. Compile only

Check:

```text
AnalyzerApp.cpp
node.cpp
shared classifier files
```

No behavior change expected yet.

---

### 2. Analyzer run, 20 trials

Look for:

```text
strong primaries:
  freq_matched=1
  tonalValid=1
  behaviorEligible=1

duplicates:
  freq_matched=0
  tonalValid=0
  behaviorEligible=0
```

---

### 3. Analyzer run, 100 trials

Compare old counters:

```text
expected_hits
late_hits
misses
unexpected
duplicates
```

Then inspect new counters:

```text
tonal_expected
non_tonal_duplicates
freq_reject_*
```

Expected result:

```text
Most expected primaries should be tonal.
Most duplicates should be non-tonal.
```

---

### 4. RB with `requireTonal=0`

Goal: behavior parity.

Expected:

```text
RB still reacts similarly to before.
Logs show tonal classification but do not block behavior.
```

---

### 5. RB with `requireTonal=1`

Goal: behavior gating test.

Expected:

```text
Real 3200 Hz chirps can trigger behavior.
Duplicates / residual amplitude events are blocked.
Blocked events include readable reasons.
```

---

## Acceptance Criteria

Pass 6 is successful when:

1. `freqScore` and `freqContrast` are used by classifier logic.
2. Strong real primaries produce:

```text
freq_matched=1
tonalValid=1
```

3. Weak duplicates/noise produce:

```text
freq_matched=0
tonalValid=0
```

4. PatternResult exposes:

```text
candidateValid
tonalValid
behaviorEligible
reason
```

5. Analyzer keeps old AMP/SEQ counters.
6. Analyzer adds classifier counters.
7. RB can optionally require tonal validity.
8. RB logs explicit block reasons.
9. AMP/transient detector behavior remains unchanged.

---

## Risk Notes

Main risk:

```text
Accidentally turning this into a detector refactor.
```

Avoid by keeping frequency validation after candidate extraction.

Second risk:

```text
Making candidates disappear from logs.
```

Avoid by keeping non-tonal candidates visible as classified candidates.

Third risk:

```text
Breaking comparability with previous SEQ runs.
```

Avoid by keeping old SEQ counters and adding new classifier counters separately.

---

## Estimated Effort

Best case:

```text
2–4 hours
```

Realistic:

```text
4–8 hours
```

If shared classifier paths are messy:

```text
1–2 days
```

The likely time sink is not the threshold logic itself.  
It is keeping Analyzer, RB, logs, and PatternResult semantics aligned.
