# Pass Y2 — SEQ Primary Pattern Selection + Source Reporting

Status: analyzer/reporting correctness pass  
Scope: Analyzer trial selection, SEQ_TRIAL, SEQ_SOURCE output  
Goal: make Analyzer trial truth select the correct primary PatternResult, then make SEQ_SOURCE explain that selected chain.

---

## Core problem

Scalar runs show impossible `late` values such as ~700ms where the expected acoustic response should be around ~20ms.

This should be treated first as a selection/accounting bug, not as scalar tuning.

Current output can mix:

```text
DetectorReport accepted.*
PatternResult / occurrence selected for trial
Analyzer classification
```

These may refer to different occurrences.

---

## 1. Primary PatternResult selection comes first

Analyzer trial classification must select:

```text
the first accepted / valid PatternResult that belongs to the active current trial window
```

More precise:

```text
primary PatternResult =
  first valid PatternResult
  whose pattern/source time is inside the active expected window
  and belongs to the current trigger/trial
```

Not:

```text
latest PatternResult
best PatternResult
last inspected occurrence
latest detector accepted item
unscoped valid PatternResult
```

If no primary is captured yet:

```text
accept first valid PatternResult inside current trial window as primary
```

If primary already exists:

```text
additional valid PatternResults in the same window count as duplicates
```

Do not let a stale scalar occurrence from before the trigger become the primary result.

---

## 2. Verify PatternResult ↔ source occurrence pairing

Check and fix pairing so that:

```text
PatternResult
PatternResult-selected source occurrence / inspected occurrence
Analyzer primaryValidPattern
Analyzer primaryValidInspectedOccurrence
SEQ_SOURCE selected source item
SEQ_TRIAL dt_ms
```

all describe the same selected chain item.

Danger pattern to remove:

```text
PatternResult popped from PatternMatcher queue
paired with _lastInspectedOccurrence
```

if `_lastInspectedOccurrence` can have changed before the PatternResult is popped.

If pairing cannot be made perfect in this pass, log/report the mismatch explicitly and do not hide it.

`SEQ_INSPECT` should prefer the inspected occurrence that belongs to the selected `PatternResult`.
If no pattern-linked occurrence exists, it should fall back to the best inspected occurrence available for the trial.

---

## 3. Add source/duplicate counts to SEQ_TRIAL

Add compact fields:

```text
dup=2 src_total=3 src_acc=1 src_rej=2
```

Meaning:

```text
dup       = duplicate valid PatternResults after the primary PatternResult
src_total = source items counted for this trial/source context
src_acc   = accepted source occurrences
src_rej   = rejected source candidates/occurrences
```

`dup` counts duplicate PatternResults, not raw occurrences.

---

## 4. SEQ_SOURCE selected item priority

`SEQ_SOURCE` should answer:

```text
Which source item explains this trial result?
```

Selection priority:

```text
1. PatternResult-selected source occurrence
2. fallback accepted occurrence
3. fallback best reject
4. none
```

`SEQ_SOURCE` must remain source-level only.

Allowed source facts:

```text
source kind
present / valid
start / peak / end-or-release / duration
strength
confidence
reject reason
source counts
```

Not allowed in `SEQ_SOURCE`:

```text
pattern evaluation internals
rule details
Analyzer classification logic beyond selected context
detector-current accepted item if it is unrelated to selected primary
```

---

## 5. Output segment labels

Target output shape:

```text
SEQ_TRIAL      ... dup=N src_total=N src_acc=N src_rej=N
SEQ_SOURCE     ... selected/generic source item ...
SEQ_SOURCE_SPEC ... detector-specific detail ...
SEQ_STAGE      ... expected / pattern / analyzer context ...
```

Disable/remove the standalone occurrence segment:

```text
SEQ_SOURCE occurrence.kind=...
```

Its useful data should be folded into the selected `SEQ_SOURCE` line.

Avoid several same-named `SEQ_SOURCE` lines with different meanings.

If `SEQ_STAGE` naming is not practical, use another clearly non-source prefix:

```text
SEQ_ANALYZER
SEQ_TRIAL_CTX
```

---

## 6. Late classification accounting

Do not change late thresholds yet.

First verify:

```text
dt_ms = selected PatternResult/source time - expected.start_ms
```

and that the selected time is the same source item printed in `SEQ_SOURCE`.

If `dt_ms` is still ~700ms after correct primary selection, then scalar detection/timing may be investigated later.

If `late` still means “after late threshold” rather than “after expected window”, rename reason wording:

```text
result_after_window
```

to:

```text
result_late_in_window
```

or:

```text
result_after_late_threshold
```

Only do this after confirming selection/accounting is correct.

---

## 7. Parity requirements

Verify both profiles:

```text
TonalPulseFreq / FrequencyMatch
TonalPulseScalar / ScalarTransient
```

Both must support:

```text
correct first primary PatternResult selection
SEQ_TRIAL dup/src_total/src_acc/src_rej
SEQ_SOURCE selected item priority
SEQ_SOURCE_SPEC detail.frequency.* or detail.scalar.*
SEQ_STAGE / equivalent context line
```

Detector-specific detail stays namespaced:

```text
detail.frequency.*
detail.scalar.*
```

---

## Guardrails

Do not change:

```text
detector thresholds
detector behavior
Occurrence emission behavior unless required to fix selected pairing
PatternMatcher validity semantics
PatternResult validity semantics
Behavior / Output
```

Avoid detector changes unless the selected PatternResult/source occurrence cannot otherwise be paired correctly.

This is primarily an Analyzer/reporting correctness pass.

---

## Output report

Create/update:

```text
docs/pass_Y2_seq_primary_selection.md
```

Include:

```text
## Primary PatternResult selection
## PatternResult/source occurrence pairing
## SEQ_TRIAL fields
## SEQ_SOURCE selected item
## Segment labels
## Late accounting verification
## FrequencyMatch parity check
## Scalar parity check
## Left for later
## Build/test result
```

---

## Acceptance

```text
- Analyzer primary trial result is the first valid PatternResult inside the active current trial window.
- Duplicates are additional valid PatternResults after the primary.
- PatternResult and selected source occurrence are paired correctly or mismatch is explicitly reported.
- SEQ_TRIAL prints dup, src_total, src_acc, src_rej.
- SEQ_SOURCE prints one selected explanatory source item.
- SEQ_SOURCE no longer mixes unrelated detector-current accepted facts with selected PatternResult occurrence.
- Standalone occurrence segment is removed/disabled.
- Detector-specific detail uses SEQ_SOURCE_SPEC.
- Expected/pattern/analyzer facts are not printed as another ambiguous SEQ_SOURCE line.
- Late dt_ms is computed from the selected primary chain item.
- TonalPulseFreq and TonalPulseScalar both produce the new fields.
- No detector threshold/tuning changes.
- Build passes.
```

Commit:

```bash
git commit -m "AnalyzerCleanup fix SEQ primary source selection"
```
