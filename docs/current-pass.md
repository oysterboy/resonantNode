# Current Pass — AMP Deviation Measurement and Support Metric Selection

## Goal

Improve the reliability of AMP support evidence by measuring robust AMP metrics before changing detection behavior.

This pass covers ranked fix-list items **5–8** only:

```text
5. Add AMP robust metric reporting.
6. Add AMP pre_event_floor and event_lift metrics.
7. Run controlled deviation comparisons before changing support behavior.
8. Replace PeakAbsolute only after measurement; likely EventP75 or EventP75MinusPreFloor.
```

## Working rule

```text
Measure first.
Do not replace PeakAbsolute blindly.
Every implemented item must be committed after implementation.
Do not stop after an individual item unless the build is broken and cannot be resolved locally.
Continue until the end of this pass list.
```

## Scope

This pass focuses on **AMP deviation**, not general detection refactor work.

In scope:

- Add robust AMP window statistics.
- Add event-relative AMP floor metrics.
- Extend Analyzer / SEQ_INSPECT output with AMP comparison values.
- Keep current behavior unchanged until measurement is available.
- Compare metrics across controlled runs.
- Add one new AMP support mode only after measurement.
- Retune AMP support thresholds only after the new metric is chosen.

Out of scope for this pass:

- FrequencyMatch detector rewrite.
- Scalar-on-frequency cleanup.
- Hann Goertzel A/B.
- Target generation handling.
- I2S backlog work.
- Analyzer staged truth rewrite.
- Behavior changes beyond AMP support metric selection.

---

# Item 5 — Add AMP robust metric reporting

## Goal

Expose robust AMP statistics for the same inspected AMP window that currently feeds `PeakAbsolute` support.

Current suspected issue:

```text
TonalPulse AMP support uses PeakAbsolute.
PeakAbsolute is sensitive to spikes, contact ticks, sharp waveform peaks, and single-bin outliers.
```

## Implementation tasks

Add AMP window statistics to the scalar/window inspection path.

At minimum report:

```text
amp.window_start_ms
amp.window_end_ms
amp.window_ms
amp.bucket_count
amp.value_count
amp.covered_ms
amp.coverage_ratio

amp.peak
amp.mean
amp.rms
amp.median
amp.p75
amp.p90
amp.trimmed_mean
```

## Notes

- Keep existing `PeakAbsolute` support decision unchanged.
- This item is measurement-only.
- `coverage_ratio` must mean true time coverage, not `valueCount / bucketCount`.
- If `trimmed_mean` is too expensive or awkward, make it optional and implement after RMS / median / p75 / p90.

## Suggested implementation order

```text
1. Add RMS support.
2. Add bounded scratch-window median / p75 / p90.
3. Add optional trimmed_mean.
4. Add fields to AMP inspection evidence / Analyzer output.
5. Confirm current PeakAbsolute remains visible.
```

## Acceptance checks

- Build succeeds.
- Existing detection behavior is unchanged.
- SEQ_INSPECT or equivalent AMP inspection output prints old and new AMP metrics.
- PeakAbsolute is still printed for comparison.
- No new metric is used for support acceptance yet.

## Commit

Commit after this item.

Suggested commit message:

```text
Add AMP robust window metrics for inspection diagnostics
```

---

# Item 6 — Add AMP pre_event_floor and event_lift metrics

## Goal

Add local acoustic floor comparison for AMP support analysis.

Important distinction:

```text
input_baseline = existing AudioSignal adaptive baseline
pre_event_floor = new local FeatureHistory window before the event
```

The new floor is not a replacement for the existing adaptive input baseline.

## Suggested windows

Event window:

```text
candidate_peak_ms - 10 ms
→ candidate_peak_ms + 90 ms
```

Pre-event floor window:

```text
candidate_peak_ms - 250 ms
→ candidate_peak_ms - 50 ms
```

If candidate peak time is not available or unreliable, use the best available occurrence time consistently and print which anchor was used.

## Metrics to compute

Add/report:

```text
amp.pre_floor_median
amp.pre_floor_p75
amp.pre_floor_rms
amp.lift_p75 = event_p75 - pre_floor_median
amp.lift_rms = event_rms - pre_floor_rms
amp.lift_trimmed_mean = event_trimmed_mean - pre_floor_median
```

Optional:

```text
amp.pre_floor_coverage_ratio
amp.pre_floor_bucket_count
amp.pre_floor_value_count
```

## Implementation notes

- Use existing `AmpEnvelope` FeatureHistory.
- Do not call this `baseline` in output unless qualified.
- Prefer names like `pre_event_floor`, `event_level`, and `event_lift`.
- Keep old `amp.lift` / same-window lift only if renamed or clearly distinguished.

## Acceptance checks

- Build succeeds.
- SEQ_INSPECT shows event-window metrics and pre-event-floor metrics.
- Existing support acceptance still uses the old mode unless explicitly configured otherwise.
- Missing/low-coverage pre-event floor is reported clearly, not silently treated as zero.

## Commit

Commit after this item.

Suggested commit message:

```text
Add AMP pre-event floor and event lift diagnostics
```

---

# Item 7 — Run controlled deviation comparisons

## Goal

Use the new metrics to determine whether AMP deviation is mostly caused by `PeakAbsolute` / metric choice, local floor changes, or real physical variation.

## Test runs

Run the same output mode and profile across:

```text
1. stable same-position
2. near
3. mid
4. far
5. movement / cable disturbance
```

For each run, collect enough trials to see spread, not just one-off behavior.

Suggested run size:

```text
50–100 trials per condition, if practical
```

## Compare these metrics

```text
amp.peak
amp.mean
amp.rms
amp.median
amp.p75
amp.p90
amp.trimmed_mean
amp.lift_p75
amp.lift_rms
amp.lift_trimmed_mean
```

For each metric, compare:

```text
median value
standard deviation
coefficient of variation
near/far separation
miss/reject correlation
spike/outlier behavior
low-coverage cases
```

## Success criteria

A good replacement metric should:

```text
- show lower spread than PeakAbsolute under stable conditions
- still separate useful support classes
- not spike randomly
- not collapse during valid events
- correlate with support presence better than raw peak
```

Expected likely candidates:

```text
event_p75
event_p75 - pre_event_floor_median
event_rms - pre_event_floor_rms
event_trimmed_mean
```

## Output summary

At the end of the comparison, write a short note into the work log or commit message body:

```text
Best candidate metric:
Rejected metrics:
Reason:
Threshold implication:
Known caveat:
```

## Commit

Commit after adding any test-output support or summary artifact.

Suggested commit message:

```text
Compare AMP support metrics across deviation test runs
```

---

# Item 8 — Replace PeakAbsolute only after measurement

## Goal

Add one new AMP support mode and retune thresholds only after Item 7 identifies the best candidate.

## Candidate support modes

Preferred first candidates:

```text
EventP75MinusPreFloor
EventP75
EventRmsMinusPreFloor
```

Avoid as long-term support truth:

```text
PeakAbsolute
```

PeakAbsolute may remain available as a diagnostic or fallback mode.

## Implementation tasks

1. Add the selected support mode to the scalar inspection mode enum / config.
2. Make TonalPulse or a test profile able to select the new mode.
3. Keep PeakAbsolute available for comparison.
4. Update SEQ_INSPECT to print:

```text
amp.support_mode
amp.classification_value
amp.support_strength
amp.threshold_weak
amp.threshold_medium
amp.threshold_strong
```

5. Retune thresholds for:

```text
weak
medium
strong
```

## Retuning rule

Do not reuse old PeakAbsolute thresholds blindly.

The new metric may have a different scale.

## Acceptance checks

- Build succeeds.
- Old PeakAbsolute mode still exists.
- New mode can be selected through profile/config.
- SEQ_INSPECT clearly prints which mode produced support classification.
- New thresholds are documented in profile config.
- Detection behavior is tested with at least one stable run after switching.

## Commit

Commit after this item.

Suggested commit message:

```text
Add measured AMP support mode and retune thresholds
```

---

# Final acceptance for the pass

The pass is complete when:

```text
- AMP robust metrics are visible.
- Pre-event floor / lift metrics are visible.
- Controlled comparison has been run or the output is ready for it.
- PeakAbsolute has not been replaced without measurement.
- If replaced, the selected mode and thresholds are documented.
- Each implemented item has its own commit.
```

## Final pass summary to write after completion

```text
Implemented:
Selected AMP metric:
Old PeakAbsolute behavior:
New thresholds:
Observed deviation improvement:
Remaining physical/acoustic limitations:
Next recommended pass:
```
