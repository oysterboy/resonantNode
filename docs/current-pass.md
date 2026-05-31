# Current Pass - Detection Safety + Deviation Preparation

## Goal

This pass prepares the detection stack for trustworthy deviation work and prevents debug/runtime mismatches from corrupting measurements.

It does not yet replace the AMP support metric or retune thresholds. It sets up the code so the next deviation pass can measure AMP / FrequencyScore behavior honestly.

## Working Rule

Each item must be implemented, compiled/tested as far as practical, and committed before moving to the next item.

Do not stop after a single successful item. Continue through the full pass list in order.

Recommended commit style:

```text
pass: <short item name>
```

Example:

```text
pass: split diagnostics reset from detector state
```

---

# Scope

## Must complete in this pass

1. Split diagnostics reset from detector/source reset.
2. Confirm Node/Analyzer runtime parity with explicit frequency config/status output.
3. Make scalar source input live-source based.
4. Preserve fresh-only frequency history and make detector reporting ms-first.
5. Fix FeatureHistory window statistic semantics if contained/small enough for this pass.

## Out of scope for this pass

Do not yet:

- replace `PeakAbsolute` as TonalPulse AMP support truth
- retune AMP thresholds
- add Hann-windowed Goertzel
- add coverage-based FrequencyMatch mode
- rewrite Analyzer stage reporting fully
- rename all semantic debt globally unless directly touched

---

# Item 1 - Split Diagnostics Reset from Detector/Source Reset

## Status

Done.

## Problem

`DetectionRuntime::resetDiagnostics()` currently resets real occurrence sources / emitters. Debug/reporting state must not affect live detection state.

If diagnostics reset can clear live candidates, all later Analyzer/deviation measurements are suspect.

## Required changes

- [x] Split diagnostics counter reset from source/detector state reset.
- [x] Introduce separate functions with clear names, for example:

```cpp
resetDiagnosticsCounters();
resetOccurrenceSources();
resetDetectionState();
```

- [x] `captureDiagnostics()` must not reset live emitters merely because diagnostics are disabled.
- [x] Profile switching / explicit runtime reset may reset occurrence sources.
- [x] Diagnostics-only reset may only clear diagnostic snapshots/counters.

## Acceptance checks

- [x] Disabling diagnostics does not reset live source candidates.
- [x] Calling diagnostics capture while diagnostics are disabled does not alter detection behavior.
- [x] Existing profile/runtime reset still has a clear path to reset emitters when explicitly intended.

## Commit after item

```text
pass: split diagnostics reset from detector state
```

---

# Item 2 - Node/Analyzer Runtime Parity Status

## Status

Done.

## Problem

Analyzer and Node may run different frequency settings. Frequency window / decimation / target must be visible in both paths before comparing behavior or debugging deviation.

## Required changes

- [x] Print or expose the same runtime frequency facts in both Analyzer and Node/RB status/logs:

```text
freq.window_samples
freq.window_ms
freq.compute_decimation
freq.update_step_ms
freq.target_hz
```

- [x] If available, also include:

```text
freq.updated_this_frame
freq.evidence_age_samples
freq.evidence_age_ms
```

## Rules

- [x] Do not assume Analyzer config equals Node config.
- [x] The runtime should make the active values visible.
- [x] If decimation can be set in Analyzer but not Node, make that difference explicit in status or add a Node-side config path if small.

## Acceptance checks

- [x] Analyzer output shows frequency config.
- [x] Node/RB status or startup output shows frequency config.
- [x] It is immediately visible whether both are using the same window and decimation.

## Commit after item

```text
pass: expose frequency runtime config parity
```

---

# Item 3 - Scalar Source Input Must Use Live Source Values

## Status

Done.

## Problem

Scalar-on-AMP already uses live frame data, but scalar-on-frequency currently risks reading from `FeatureHistory.latestValue()`. Source detectors should consume live/current source values. Inspectors should consume retrospective FeatureHistory windows.

Correct split:

```text
Source path = live stream
Inspector path = FeatureHistory window
```

## Required changes

- [x] Change scalar source selection so it receives both the current `AudioSignalFrame` and the current `FrequencyFeatureFrame`.

Target behavior:

```text
Scalar source on AmpEnvelope:
  input = frame.centeredMagnitude

Scalar source on FrequencyScore:
  input = frequencyFrame.score

Scalar source on FrequencyContrast:
  input = frequencyFrame.spectralContrast

Scalar source on AmbientFloor:
  input = frame.baseline
```

- [x] Do not use `history.latestValue(FrequencyScore)` or `history.latestValue(FrequencyContrast)` as source detector input.

## Held/fresh rule for this pass

- [x] Do not over-refactor freshness gating here.

Allowed for now:

```text
frequencyFrame may expose held latest score/contrast between fresh updates
scalar-on-frequency may consume the live frequencyFrame value
```

Required:

```text
scalar source input no longer depends on FeatureHistory
```

Fresh-only FeatureHistory remains for inspectors.

## Optional small cleanup

If easy, add a helper:

```cpp
bool isFrequencyDerivedStream(FeatureStreamId stream);
```

Do not rename `AmpTransient` globally in this item unless trivial. That is later cleanup.

## Acceptance checks

- [x] Scalar-on-AMP still uses `frame.centeredMagnitude`.
- [x] Scalar-on-frequency uses `frequencyFrame.score` / `frequencyFrame.spectralContrast` directly.
- [x] FeatureHistory is not used to provide scalar source input.
- [x] Inspectors still use FeatureHistory windows.

## Commit after item

```text
pass: make scalar source input live-source based
```

---

# Item 4 - Preserve Fresh-only Frequency History + Make Detector Reporting ms-first

## Status

Done.

## Problem

Fresh-only FeatureHistory appears to be the right model. The detector may still consume held live values, which is acceptable while decisions are wall-time based. The risk is mostly reporting/interpretation: counts must not be presented as independent evidence.

## Required changes

- [x] Confirm and preserve:

```text
FrequencyFeatureFrame has updatedThisFrame / freshness marker.
FeatureHistory records FrequencyScore / FrequencyContrast only when updatedThisFrame is true.
Inspector windows over frequency streams therefore use fresh frequency bins only.
```

- [x] Adjust diagnostics/reporting where practical:

```text
candidate.duration_ms = primary timing truth
release_gap_ms = primary release truth
matched_span_ms = preferred over raw frame/window counts
```

- [x] Counts may remain, but label them honestly:

```text
observed_calls
matched_calls
score_ok_calls
contrast_ok_calls
```

- [x] Avoid implying:

```text
independent windows
independent evidence frames
```

## Acceptance checks

- [x] Frequency history is still fresh-only.
- [x] Detector acceptance remains ms-based.
- [x] Any printed count fields are clearly secondary and not named as independent evidence if they include held calls.
- [x] No behavior retuning in this item.

## Commit after item

```text
pass: keep frequency history fresh and detector reports ms-first
```

---

# Item 5 - Fix FeatureHistory Window Statistic Semantics

## Status

Done.

## Problem

Some FeatureHistory/ScalarWindow fields currently sound physically meaningful but are ambiguous or misleading.

Known issue:

```text
coverageRatio = valueCount / bucketCount
```

This is not true time coverage. For AMP, many values may land in one millisecond bucket, so this can exceed 1 or behave like values-per-bucket.

## Required changes

- [x] Split window statistics into explicit fields:

```text
valueCount        // number of recorded values
bucketCount       // number of ms bins with data
valuesPerBucket   // density, if useful
coveredMs         // number of represented ms bins or covered time
coverageRatio     // coveredMs / requestedWindowMs
```

- [x] Avoid using `freshValueCount` generically for AMP. AMP density and frequency freshness are different concepts.

- [x] If a full rename is too large, add new fields and leave old fields as deprecated/compat for now.

## Rules

- [x] Inspector windows remain time-window based.
- [x] AMP and frequency can share the same outer window shape.
- [x] Frequency-specific freshness remains represented by fresh-only recording and/or frequency-specific diagnostics.

## Acceptance checks

- [x] `coverageRatio` means time coverage, not value density.
- [x] AMP windows can report both dense value count and time coverage.
- [x] Frequency windows can report fresh-bin coverage without pretending to have AMP-like density.
- [x] Existing inspector logic still compiles and produces equivalent decisions unless explicitly changed.

## Commit after item

```text
pass: clarify feature history window statistics
```

---

# End-of-pass checks

After all items are complete, run a short sanity test and inspect output for:

```text
- diagnostics reset does not affect detection
- Analyzer and Node show same frequency config values where expected
- scalar-on-frequency source input no longer uses FeatureHistory.latestValue
- frequency FeatureHistory remains fresh-only
- detector outputs prioritize ms timing
- FeatureHistory window stats have clear semantics
```

Do not start AMP support replacement until this pass is done.

## Status

All current-pass items are checked off in the doc. Remaining work, if any, belongs in the next pass preview or in follow-up cleanup commits.

---

# Next Pass Preview - Signal Deviation Measurement

After this current pass, the next pass should focus on improving signal deviation measurement:

```text
1. Add AMP robust metrics: peak, mean, RMS, median, p75, p90, trimmed_mean.
2. Add AMP pre_event_floor and event_lift metrics.
3. Run controlled deviation comparisons.
4. Replace PeakAbsolute only after data proves the better support metric.
```
