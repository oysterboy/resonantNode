# Canonical SEQ_INSPECT

## Purpose

Add canonical detector-stage inspect / explain output that reads frozen
contracts instead of reconstructing truth from legacy diagnostic dumps.

This pass also renames the existing legacy inspect / explain labels so both
surfaces can coexist:

- canonical: `SEQ_INSPECT`, `SEQ_EXPLAIN`
- legacy: `SEQ_INSPECT_LEG`, `SEQ_EXPLAIN_LEG`

## Source Contracts

Canonical inspect/explain now read from:

- `DetectorReport`
- `SelectedRejectSummary`
- `AnalyzerReport.primaryPattern` as the frozen `PatternResult` summary bridge
- `AnalyzerReport.expected`

Analyzer still prints the legacy surfaces separately through the old
compatibility path.

## Output Shape

Canonical `SEQ_INSPECT` / `SEQ_EXPLAIN` now follow this structure:

1. generic detector report line
2. detector-specific detail line selected by `report.detectorId`
3. stage summary line for expected window / pattern / analyzer result

Generic contract fields print first:

```text
detector
window.*
accepted.*
reject.*
threshold.*
aggregate.*
```

Detector-specific detail prints next:

```text
detail.scalar.*
detail.frequency.*
```

Dispatch rule:

```text
DetectorId::ScalarTransient -> detail.scalar.*
DetectorId::FrequencyMatch -> detail.frequency.*
```

The analyzer does not infer detector type by checking whether one detail block
happens to contain values.

## DetectorReport Fields Used

Generic fields used:

- `detectorId`
- `reportStartMs`
- `reportEndMs`
- `accepted.present`
- `accepted.startMs`
- `accepted.peakMs`
- `accepted.endMs`
- `accepted.durationMs`
- `accepted.strength`
- `accepted.confidence`
- `selectedReject.present`
- `selectedReject.rejectClass`
- `selectedReject.detectorReason`
- `selectedReject.startMs`
- `selectedReject.peakMs`
- `selectedReject.endMs`
- `selectedReject.durationMs`
- `selectedReject.strength`
- `selectedReject.confidence`
- `thresholds.minDurationMs`
- `thresholds.maxDurationMs`
- `aggregates.acceptedCount`
- `aggregates.rejectedCount`

Scalar detail fields used:

- `scalar.accepted.*`
- `scalar.selectedReject.*`
- `scalar.thresholds.*`
- `scalar.aggregates.*`
- `scalar.inspect.*`

Frequency detail fields used:

- `frequency.accepted.*`
- `frequency.selectedReject.*`
- `frequency.thresholds.*`
- `frequency.aggregates.*`
- `frequency.inspect.*`

## RejectedCandidateSummary Fields Used

Canonical printers use the generic selected-reject shell through:

- `reject.present`
- `reject.class`
- `reject.detector_reason`
- `reject.start_ms`
- `reject.peak_ms`
- `reject.end_ms`
- `reject.duration_ms`
- `reject.strength`
- `reject.confidence`

Detector-specific reject detail remains namespaced under:

- `detail.scalar.reject.*`
- `detail.frequency.reject.*`

## PatternResult Fields Used

The analyzer already stores the frozen pattern summary in `primaryPattern`.

Canonical inspect/explain use:

- `pattern.type`
- `pattern.valid`
- `pattern.reason`

And the analyzer stage summary uses:

- `analyzer.result`
- `analyzer.reason`
- `analyzer.dt_ms`

## Legacy Output Compatibility

Legacy output is retained, but explicitly relabeled:

- `SEQ_INSPECT` -> `SEQ_INSPECT_LEG`
- `SEQ_INSPECT_COMPARE` -> `SEQ_INSPECT_LEG_COMPARE`
- legacy deep explain header now emits `SEQ_EXPLAIN_LEG`

Mode routing after this pass:

- `inspect` -> canonical `SEQ_INSPECT`
- `explain` -> canonical `SEQ_EXPLAIN`
- `LEG_inspect` -> legacy inspect
- `LEG_explain` / `LEG_dump` -> legacy explain

`Explain` no longer implicitly means the legacy source / system dump bundle.
That old deep bundle remains under the explicit legacy explain mode.

## Scalar Coverage

Scalar canonical coverage is strong in this pass:

- generic accepted / reject / threshold / aggregate fields
- scalar accepted detail
- scalar selected-reject detail
- scalar threshold / aggregate detail
- scalar inspect lifecycle detail

## Frequency Coverage

Frequency canonical coverage is now present and useful:

- generic accepted / reject / threshold / aggregate fields
- frequency accepted detail
- frequency selected-reject detail
- frequency threshold / aggregate detail
- frequency inspect lifecycle detail

The canonical printer only uses frequency fields that actually exist in the
current `DetectorReport`.

## Remaining Gaps

- canonical inspect/explain still do not replace legacy `SEQ_SOURCE`
- analyzer still keeps a `DetectionDiagnostics` fallback for richer
  legacy-only frequency fields outside the frozen `DetectorReport`
- canonical explain still uses the analyzer’s frozen pattern summary bridge
  rather than a more explicit canonical pattern print contract
- legacy candidate logs remain legacy-only

## Recommended Next Pass

Recommended next pass: `Pass L - Move analyzer trial truth to canonical inputs`.
