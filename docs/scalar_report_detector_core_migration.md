# Scalar Report Detector-Core Migration

## Purpose

Document the Pass F ownership move that shifts canonical scalar `DetectorReport`
facts closer to `ScalarTransientDetector` without changing scalar detection
behavior, Analyzer output, or legacy diagnostics compatibility.

## Previous Temporary Bridge

Before Pass F, scalar report production still rebuilt canonical report truth
from `ScalarOccurrenceSource` getters:

```text
ScalarTransientDetector
-> ScalarOccurrenceSource
-> DetectionRuntime::refreshScalarDetectorReport(...)
-> DetectorReport
```

That meant detector-stage canonical report fields still depended on wrapper-held
candidate lifecycle and selected-reject summary data.

## Pass E Analyzer Bridge Assumption

Pass E already moved Analyzer scalar report synthesis onto:

```text
DetectionRuntime::scalarDetectorReport()
```

So Pass F focuses only on report production ownership, not Analyzer
consumption.

## New Report Ownership

After Pass F, canonical scalar report production is split like this:

```text
ScalarTransientDetector
-> owns ScalarDetectorReportDetail
-> owns canonical selected RejectedCandidateSummary
-> DetectionRuntime::refreshScalarDetectorReport(...)
-> DetectorReport
```

`DetectionRuntime` still assembles the outer `DetectorReport`, but the
canonical scalar detail and canonical selected reject now come from
`ScalarTransientDetector` rather than wrapper-specific getters.

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

Pass F keeps:

- `DetectionRuntime::scalarDetectorReport()` unchanged as the public accessor
- scalar legacy fallback population intact
- scalar legacy aggregate summaries intact

The canonical scalar detail copied into legacy scalar diagnostics is now sourced
from detector-owned report detail instead of wrapper reconstruction.

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

`DetectionRuntime` also still assembles the outer `DetectorReport`, and accepted
occurrence facts still come from runtime-held emitted occurrence state rather
than directly from the detector core.

The `TEMP_SCALAR_REPORT_BRIDGE` comment remains because the wrapper is still in
the scalar report path for occurrence emission and legacy aggregate compatibility
data, even though canonical scalar detail and canonical selected reject are now
detector-owned.

## Recommended Next Pass

Recommended next pass:

- `Pass G - Remove ScalarOccurrenceSource from Scalar Report Path`

If detector-owned accepted-occurrence facts prove to be the missing step first,
the intermediate fallback remains:

- `Pass F2 - Add Missing ScalarTransientDetector Report Facts`
