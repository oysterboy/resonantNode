# Generic Detector Report Refresh Boundary

## Purpose

Clarify the code boundary for detector report production so `DetectionRuntime`
coordinates detectors without becoming the long-term owner of detector-specific
report assembly.

## Problem

The earlier scalar migration bridge used a detector-specific runtime helper:

```text
DetectionRuntime::refreshScalarDetectorReport()
```

That was useful during migration, but it would be the wrong pattern if copied
per detector type.

`DetectionRuntime` must not grow one `refreshXXDetectorReport()` function per
detector type.

## Detector Genericity Rule

`Detector` is a shared architectural role, not necessarily one forced C++ base
class yet.

The shared outward detector contract is:

- stable `DetectorId` / `DetectorDescriptor`
- accepted `Occurrence` emission
- `DetectorReport` exposure
- selected rejected candidate exposure through `RejectedCandidateSummary`
- generic reject class through `DetectorRejectClass`

Detector-specific internals may remain specialized:

- feature input type
- update method shape
- candidate lifecycle state
- lifecycle implementation
- detector-specific reject reasons
- typed report detail
- typed occurrence detail

This does not require a forced `IDetector` base class or type-erased feature
input yet.

## Previous Scalar Refresh Path

Before this pass, the active scalar report path looked like:

```text
ScalarTransientDetector
-> accepted/detail/reject getters
-> DetectionRuntime::refreshScalarDetectorReport(...)
-> DetectorReport
-> DetectionRuntime::scalarDetectorReport()
```

Canonical scalar facts were already detector-owned by Pass G, but
`DetectionRuntime` still assembled the scalar-specific outer `DetectorReport`
field-by-field.

## New Scalar Report Ownership

After this pass, scalar report assembly is detector-local:

```text
ScalarTransientDetector::buildReport(...)
-> DetectionRuntime::refreshDetectorReports(...)
-> DetectionRuntime::scalarDetectorReport()
-> Analyzer scalar bridge
```

`ScalarTransientDetector` now builds the canonical scalar `DetectorReport`
snapshot, including:

- accepted occurrence summary
- scalar detector detail
- selected rejected candidate summary
- report window start/end selection

`DetectionRuntime` no longer maps those scalar-specific report fields itself.

## DetectionRuntime Responsibility After This Pass

`DetectionRuntime` now stays closer to coordinator role:

- update the active detector path
- drain accepted occurrences
- ask active detectors to refresh report snapshots
- expose the current scalar report through `scalarDetectorReport()`
- copy legacy compatibility fields into `DetectionDiagnostics` where still needed

Detector-specific report construction belongs to detector cores or
detector-local helpers, not to detector-specific runtime refresh functions.

## Runtime Report Access Implication

The current runtime report accessor is still scalar-specific:

```text
DetectionRuntime::scalarDetectorReport()
```

There is no generic runtime report accessor yet such as:

```text
detectorReport(DetectorId)
activeDetectorReport()
```

This is acceptable as a migration step, but it should be treated as temporary
scalar naming rather than the template for future detector report access.

Frequency migration should not automatically copy this into:

```text
DetectionRuntime::frequencyDetectorReport()
```

unless a later pass explicitly decides that per-detector report accessors are
the final outward pattern.

## Frequency Migration Implication

Frequency migration should not copy the old scalar bridge shape into:

```text
DetectionRuntime::refreshFrequencyDetectorReport()
```

Instead, frequency should follow the same outward rule:

- detector-owned accepted/rejected truth
- detector-owned typed report detail
- generic `DetectorReport` exposure upward

with frequency-specific input/update internals still free to remain specialized.

## What Did Not Change

This pass does not:

- migrate `FrequencyMatchDetector`
- introduce a forced `IDetector` base class
- introduce type-erased detector feature input
- change scalar detection behavior
- change thresholds, timing, or profiles
- redesign Analyzer output
- remove `DetectionDiagnostics`

`DetectionRuntime::scalarDetectorReport()` remains the stable scalar report
accessor used by the Analyzer bridge.

## Remaining Temporary Bridges

Temporary bridge work still remains in the scalar path:

- `ScalarTransientDetector` now owns scalar accepted `Occurrence` emission and
  detector-local report production
- `DetectionRuntime` still stores the scalar report snapshot and copies legacy
  scalar compatibility values into `DetectionDiagnostics`
- `ScalarTransientDetector` now also carries temporary legacy scalar
  rejected-candidate aggregate compatibility data for
  `DetectionDiagnostics` / Analyzer fallback
- runtime report access is still scalar-specific through
  `scalarDetectorReport()`
- frequency still has no migrated `DetectorReport` path

## Recommended Next Pass

Recommended next pass:

- `Pass I - Begin FrequencyMatch DetectorReport Migration`

Scalar wrapper cleanup no longer blocks detector parity work because
`ScalarOccurrenceSource` was removed in Pass H2.
