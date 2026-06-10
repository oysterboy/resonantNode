# Analyzer Scalar Report Bridge

## Purpose

Document the Pass E bridge that makes scalar Analyzer report synthesis consume the canonical scalar `DetectorReport` while keeping legacy SEQ output stable.

## Previous Scalar Analyzer Source

Before Pass E, scalar Analyzer report synthesis read scalar detector truth from the legacy runtime dump:

```text
ScalarTransientDetector
-> ScalarOccurrenceSource
-> DetectionRuntime::captureDiagnostics()
-> DetectionDiagnostics.scalar* + sourceSummary/sourceLastCandidate
-> AnalyzerApp::buildSequenceAnalyzerReport()
-> AnalyzerScalarDiagnostic / AnalyzerSourceStageReport
-> AnalyzerLegacyReporting print helpers
```

## New Scalar Analyzer Source

Pass E makes `AnalyzerApp::buildSequenceAnalyzerReport()` read the overlapping scalar detector-truth fields from:

```text
DetectionRuntime::scalarDetectorReport()
-> DetectorReport
```

The active bridge is now:

```text
Scalar DetectorReport
-> AnalyzerApp::buildSequenceAnalyzerReport()
-> AnalyzerScalarDiagnostic / AnalyzerSourceStageReport
-> AnalyzerLegacyReporting print helpers
```

## Fields Now Populated from DetectorReport

The scalar Analyzer source report now uses `DetectorReport` as the primary source for overlapping scalar detector truth:

- `acceptedPresent`
- accepted timing
  - `acceptedStartMs`
  - `acceptedPeakMs`
  - `acceptedReleaseMs`
  - `acceptedDurationMs`
- accepted detector values
  - `acceptedStrength`
  - `acceptedScore`
  - `acceptedContrast`
- scalar lifecycle state
  - `scalarRejectReason`
  - `scalarNoEmitReason`
  - `scalarGateReason`
  - `scalarOpened`
  - `scalarReleased`
  - `scalarValidRelease`
  - `scalarEmitAllowed`
  - `scalarOpenMs`
  - `scalarPeakMs`
  - `scalarReleaseMs`
  - `scalarDurationMs`
  - `scalarMinDurationMs`
  - `scalarMaxDurationMs`
  - `scalarPeakStrength`
- selected reject overlap used to synthesize legacy scalar summary fields
  - `sourceSummary.bestDurationMs`
  - `sourceSummary.bestOpenMs`
  - `sourceSummary.bestPeakMs`
  - `sourceSummary.bestLastMatchMs`
  - `sourceSummary.bestCloseMs`
  - `sourceSummary.bestPeakPrimary`
  - `sourceSummary.bestRejectReason`
  - `sourceLastCandidate.peakMs`
  - `sourceLastCandidate.durationMs`
  - `sourceLastCandidate.peakPrimary`
  - `sourceLastCandidate.reason`

## Fields Still Populated from DetectionDiagnostics

Scalar Analyzer output still depends on `DetectionDiagnostics` for fallback or legacy-only fields that `DetectorReport` does not represent yet:

- scalar fallback when the runtime report is unavailable
- selected reject gate-reason fallback
- legacy aggregate leftovers in synthesized scalar summaries
  - `sourceSummary.maxPeakPrimary`
  - `sourceSummary.maxPeakPrimaryMs`
  - `sourceSummary.totalGapMs`
  - `sourceSummary.maxGapMs`
- any remaining legacy-only scalar wording that is not represented directly in `DetectorReport`

Frequency Analyzer fields still remain fully legacy and continue to read `DetectionDiagnostics`.

## Legacy Output Compatibility

Pass E does not change:

- SEQ mode names
- printed field names
- printed field order
- Analyzer classification logic
- frequency output structure

The goal of this pass is the same legacy output surface with a cleaner scalar data source underneath.

## What Did Not Change

This pass does not:

- migrate frequency Analyzer fields
- add frequency `DetectorReport` detail
- remove `DetectionDiagnostics`
- remove Analyzer legacy structs
- remove `ScalarOccurrenceSource`
- rewire `DetectionRuntime`
- redesign SEQ output

## Remaining Gaps

- scalar Analyzer synthesis still needs `DetectionDiagnostics` for some legacy-only fallback fields
- selected reject gate and aggregate leftovers are not fully canonical yet
- frequency Analyzer synthesis still does not consume `DetectorReport`
- scalar report production is now detector-local, but scalar `Occurrence` emission and some legacy diagnostics still remain behind the temporary `ScalarOccurrenceSource` bridge

## Validation

- `esp32dev-analyzer` build passes after the Pass E bridge changes
- short SEQ sanity confirmed the stale scalar accepted-state regression was fixed for `Amp`
- `TonalPulse` still produced clean expected hits in the short sanity run, so the frequency-match path stayed stable during this pass
- shared `AmpEnvelope` retuning moved `Amp` and `ChirpExperimental` from empty `below_threshold` misses to real scalar candidates with valid durations, but those candidates still failed the peak gate as `strength_too_low`
- `Amp` now carries an additional profile-local peak gate reduction to `minTransientPeakStrength = 28.0f`; that last tweak was intentionally not rerun in this pass
- `scalar_freq_experimental` still reproduces `Late`, and the explain trace shows why: an earlier in-window frequency-score burst is rejected as `duration_too_long`, then a later shorter burst becomes the accepted late hit
- the final Analyzer bridge fix now treats frequency-backed scalar profiles as accepted when their runtime occurrence source is `frequency`, so legacy scalar source reporting no longer hard-codes `amp`; this final reporting correction was compile-checked but not flashed or rerun on hardware in this pass

## Pass E Outcome

- the main Pass E goal was achieved: overlapping scalar Analyzer detector-truth fields now come from `DetectorReport`
- legacy SEQ output stayed in place while the scalar bridge moved underneath it
- `DetectionDiagnostics` remains the fallback and legacy-only source where `DetectorReport` still has gaps
- the frequency path was not migrated, but the pass surfaced a real follow-up issue in the experimental frequency-backed scalar profile timing
- runtime validation also exposed that the scalar AMP path needed profile-local retuning before it could emit cleanly again

## Recommended Next Pass

Recommended next pass:

- `Pass F - Move Scalar Report Production Toward ScalarTransientDetector`

If Analyzer bridge review shows missing scalar fields, the fallback alternative remains:

- `Pass E2 - Fill Scalar DetectorReport Gaps`

Immediate follow-up items from the Pass E runtime checks:

- rerun `Amp` after the `28.0f` peak-gate change
- retune `scalar_freq_experimental`, most likely around its scalar transient duration window or the frequency-derived source semantics it is currently testing
