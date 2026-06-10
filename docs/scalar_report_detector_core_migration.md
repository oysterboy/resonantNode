# Scalar Report Detector-Core Migration

## Purpose

Document the scalar report ownership move that started in Pass F and now places
canonical scalar `DetectorReport` assembly in detector-local code without
changing scalar detection behavior, Analyzer output, or legacy diagnostics
compatibility.

## Previous Temporary Bridge

Before the detector-core migration completed, scalar report production rebuilt
canonical report truth through a scalar-specific runtime bridge:

```text
ScalarTransientDetector
-> accepted/detail/reject getters
-> DetectionRuntime::refreshScalarDetectorReport(...)
-> DetectorReport
```

That meant `DetectionRuntime` still assembled the outer scalar `DetectorReport`
field-by-field even after more canonical facts had moved into the detector.

## Pass E Analyzer Bridge Assumption

Pass E already moved Analyzer scalar report synthesis onto:

```text
DetectionRuntime::scalarDetectorReport()
```

So Pass F focuses only on report production ownership, not Analyzer
consumption.

## New Report Ownership

After Pass G2b, canonical scalar report production is:

```text
ScalarTransientDetector
-> owns accepted occurrence summary
-> owns ScalarDetectorReportDetail
-> owns canonical selected RejectedCandidateSummary
-> buildReport(...)
-> DetectionRuntime::refreshDetectorReports(...)
-> DetectorReport
```

`DetectionRuntime` still stores the scalar report snapshot for the stable
`scalarDetectorReport()` accessor, but detector-local code now assembles the
scalar-specific report fields.

## ScalarTransientDetector Facts Now Used Directly

`ScalarTransientDetector` now owns and exposes the canonical scalar detail used
by `DetectorReport.scalarTransient`:

- reject / no-emit / gate reason
- opened / released / validRelease / emitAllowed
- open / peak / release timing
- current candidate duration
- configured min / max duration
- current candidate peak strength

`ScalarTransientDetector` also now owns the canonical selected rejected
candidate used by `DetectorReport.selectedReject`:

- `selectedRejectPresent`
- reject class
- detector reason
- rejected candidate start / peak / end
- rejected candidate duration
- required min / max duration
- rejected peak strength

The detector keeps the same `>= duration` selection rule that the old wrapper
summary used for the chosen rejected candidate, so this ownership move does not
change the selection policy.

`ScalarTransientDetector` also now owns detector-local assembly of:

- `DetectorReport.detectorId`
- `DetectorReport.acceptedPresent`
- `DetectorReport.acceptedOccurrence`
- `DetectorReport.selectedRejectPresent`
- `DetectorReport.selectedReject`
- `DetectorReport.reportStartMs`
- `DetectorReport.reportEndMs`

## ScalarOccurrenceSource Facts Still Used

`ScalarOccurrenceSource` still remains in the scalar path for:

- `Occurrence` emission
- accepted occurrence payload construction
- legacy aggregate reject summary values used by `DetectionDiagnostics`
  - reject counts
  - best / second-best rejected duration summary
  - rejected gap totals / max gap / island count
  - max rejected peak strength aggregates
- legacy last-candidate compatibility fields

Pass F does not remove those wrapper-owned aggregate leftovers.

## Selected Reject Ownership

Canonical selected reject ownership moved from the wrapper path into
`ScalarTransientDetector`.

This means:

- `DetectorReport.selectedRejectPresent`
- `DetectorReport.selectedReject`

now come from detector-owned facts.

Legacy aggregate reject diagnostics still use wrapper-owned summary data until a
later pass.

## DetectionDiagnostics Compatibility

`DetectionDiagnostics` remains active and compatible.

The current migration state keeps:

- `DetectionRuntime::scalarDetectorReport()` unchanged as the public accessor
- scalar legacy fallback population intact
- scalar legacy aggregate summaries intact

The canonical scalar detail copied into legacy scalar diagnostics is now sourced
from detector-built report snapshots instead of runtime scalar field mapping.

## Analyzer Compatibility

Analyzer compatibility from Pass E remains intact:

- Analyzer scalar report synthesis still reads `scalarDetectorReport()`
- legacy SEQ output structure stays unchanged
- frequency Analyzer fields remain legacy

No Analyzer-facing API or field names changed in this pass.

## Runtime / Tuning Items Explicitly Deferred

This pass intentionally does not change runtime tuning behavior.

Explicitly deferred:

- Amp rerun / tuning follow-up is not part of this pass.
- `scalar_freq_experimental` timing / `duration_too_long` follow-up is not part
  of this pass.
- no threshold changes
- no duration-window changes
- no peak-gate changes
- no profile default changes

## What Did Not Change

Pass F does not:

- change scalar detector accept / reject behavior
- change occurrence timing or strength semantics
- delete `ScalarOccurrenceSource`
- delete `DetectionDiagnostics`
- migrate frequency `DetectorReport` production
- redesign Analyzer output

## Remaining Bridge / Deletion Blockers

`ScalarOccurrenceSource` still cannot be deleted yet because it still owns:

- scalar `Occurrence` emission
- accepted occurrence payload construction
- legacy aggregate reject / source-summary compatibility values

`DetectionRuntime` no longer assembles scalar report fields directly, but it
still:

- stores the scalar report snapshot
- refreshes detector reports as a coordinator step
- copies legacy scalar compatibility values into `DetectionDiagnostics`

## Recommended Next Pass

Recommended next pass:

- `Pass H - Route Scalar Occurrence Emission Directly from ScalarTransientDetector`

If scalar report ownership review still reveals cleanup gaps, the fallback is:

- `Pass G3 - Finish Scalar DetectorReport Ownership Cleanup`
