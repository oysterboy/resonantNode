# Pass X6 - FrequencyMatch / Scalar Detector Parity Check

Status: inspection pass
Scope: detector + analyzer bridge only
Goal: verify that `FrequencyMatchDetector` and `ScalarTransientDetector` expose equivalent canonical contracts.

## Scalar path

`ScalarTransientDetector` owns the scalar-stream lifecycle from onset detection through accepted occurrence emission.

Canonical outputs and ownership:
- Builds `DetectorReport` directly in `buildReport()`
- Owns the accepted-occurrence shell through `AcceptedOccurrenceSummary`
- Owns the detector-specific reject shell through `SelectedRejectSummary`
- Owns scalar-specific report detail through `ScalarDetectorReportDetail`
- Emits accepted `Occurrence` values through `popOccurrence()`

Relevant implementation facts:
- Accepted occurrences are captured in `_acceptedOccurrence` and mirrored into `_pendingOccurrence`
- Selected reject state is tracked separately in `_selectedReject`
- `DetectorReport` exposes the generic shell plus `scalar` detail
- `reportStartMs` / `reportEndMs` are derived from accepted, open, or selected-reject state in that order

## FrequencyMatch path

`FrequencyMatchDetector` owns the frequency-stream lifecycle from gate evaluation through accepted occurrence emission.

Canonical outputs and ownership:
- Builds `DetectorReport` directly in `buildReport()`
- Owns the accepted-occurrence shell through `AcceptedOccurrenceSummary`
- Owns the detector-specific reject shell through `SelectedRejectSummary`
- Owns frequency-specific report detail through `FrequencyMatchDetectorReportDetail`
- Emits accepted `Occurrence` values through `popOccurrence()`

Relevant implementation facts:
- Accepted occurrences are captured in `_acceptedOccurrence` and `_acceptedDetail`
- The detector keeps pending lifecycle state and a separate best-reject snapshot
- Selected reject is derived from the best rejected pending lifecycle, not just the last one
- `DetectorReport` exposes the generic shell plus `frequency` detail
- `reportStartMs` / `reportEndMs` are derived from accepted, open, or selected-reject state in that order

## Parity table

| Contract | ScalarTransientDetector | FrequencyMatchDetector | Parity note |
| --- | --- | --- | --- |
| Canonical report builder | `buildReport()` | `buildReport()` | Parity |
| Accepted shell | `AcceptedOccurrenceSummary` | `AcceptedOccurrenceSummary` | Parity |
| Selected reject shell | `SelectedRejectSummary` | `SelectedRejectSummary` | Parity |
| Accepted occurrence emission | `popOccurrence()` | `popOccurrence()` | Parity |
| Detector-specific detail namespace | `scalar` | `frequency` | Parity by namespacing |
| Threshold reporting | scalar thresholds | frequency thresholds | Family-specific but symmetric |
| Aggregate counters | scalar aggregate counts | frequency aggregate counts | Family-specific but symmetric |
| Inspect/gate facts | scalar inspect facts | frequency inspect facts | Family-specific but symmetric |
| Runtime report exposure | `DetectionRuntime::activeDetectorReport()` | same | Parity |
| Analyzer canonical read path | `DetectorReport` + `Occurrence`/`InspectedOccurrence` | same | Parity |

## Remaining asymmetries

- `FrequencyMatchDetector` tracks a best rejected pending lifecycle before it commits `SelectedRejectSummary`.
- `ScalarTransientDetector` commits selected reject from the scalar transient rejection path directly.
- `DetectionRuntime` still snapshots `_lastOccurrence` and `_lastInspectedOccurrence` into `DetectionPipelineResult`, so the latest pipeline view is not rebuilt purely from `DetectorReport`.
- `AnalyzerApp` still reads `InspectedOccurrence` carrier fields for display details, especially `occurrence.scalar.*` and `occurrence.frequency.*`.
- `AnalyzerReporting` prints detector-specific detail under `detail.scalar.*` and `detail.frequency.*`, which is separate from the canonical generic shell.

## Which asymmetries are acceptable

- Family-specific report detail under `detail.scalar.*` and `detail.frequency.*`
- Different detector-internal lifecycle bookkeeping as long as `DetectorReport` remains canonical
- `DetectionRuntime` keeping a last-occurrence and last-inspected snapshot for trial reporting
- Analyzer reading `InspectedOccurrence` payload fields for presentation only

## Which asymmetries need cleanup

- If a later pass wants a stricter bridge, `DetectionRuntime` could stop carrying last-occurrence truth outside the canonical report path and instead expose only the report-backed data needed by analyzer consumers.
- If a later pass wants tighter detector parity, `FrequencyMatchDetector` could simplify its selected-reject selection policy, but that would be a behavioral cleanup, not a parity requirement.

## Recommended next pass

Focus on the remaining bridge asymmetry:
- audit whether `DetectionPipelineResult` still needs both `_lastOccurrence` and `_lastInspectedOccurrence`
- confirm that analyzer display code can stay on canonical report fields plus `InspectedOccurrence` payload facts
- keep `DetectorReport` as the single detector-stage report contract and avoid rebuilding detector truth in `DetectionRuntime`

