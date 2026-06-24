# Detection History Coverage Audit

This audit covers the active inspection modules in `src/detection/DetectionProfile.h` and whether their synchronous inspection windows are normally retrospective when the occurrence is drained.

## Active windows

| Profile | Module target | Stream | Anchor | preMs | postMs | Typical inspection moment | Expected complete |
|---|---|---:|---|---:|---:|---|---|
| `TonalPulseScalar` | `Contrast` | `FrequencyContrast` | `Start` | 0 | 100 | Candidate close / detector drain | Conditional |
| `TonalPulseScalar` | `Amp` | `AmpMagnitude` | `Start` | 0 | 100 | Candidate close / detector drain | Conditional |
| `TonalPulseFreq` | `Amp` | `AmpEnvelope` | `Peak` | 10 | 90 | Pattern match / detector drain | Conditional |
| `TonalPulseFreq` | `TargetScore` | `FrequencyTarget` | `Peak` | 10 | 90 | Pattern match / detector drain | Conditional |
| `TonalPulseFreq` | `Contrast` | `FrequencyContrast` | `Peak` | 10 | 90 | Pattern match / detector drain | Conditional |
| `AmpExperimental` | `Amp` | `AmpEnvelope` | `Peak` | 10 | 90 | Candidate close / detector drain | Conditional |
| `AmpExperimental` | `TargetScore` | `FrequencyTarget` | `Peak` | 10 | 90 | Candidate close / detector drain | Conditional |
| `AmpExperimental` | `Contrast` | `FrequencyContrast` | `Peak` | 10 | 90 | Candidate close / detector drain | Conditional |

## Coverage semantics

- `coverageComplete=1` means the requested window had values and the inspected data covered the requested interval without gaps, and the request did not extend beyond the inspection time.
- `future_unavailable=1` means `requestedEndMs > inspectionNowMs` at the time of inspection.
- `covered_ms` is derived from the fixed 1 ms history-bin contract used by `FeatureHistory`.
- `sustained_ms` follows the same fixed-bin timing contract.

## Deferred cases

The active profiles remain synchronous, but they are only fully retrospective when the detector drain happens after the window end:

- Peak-anchored windows are complete only when the occurrence stays active long enough after the peak.
- Start-anchored windows are complete only when the occurrence stays active long enough after `windowPostMs`.

Deferred `WaitForComplete` behavior is not introduced in this pass.
