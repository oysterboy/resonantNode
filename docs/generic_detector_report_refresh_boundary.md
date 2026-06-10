# Generic Detector Report Refresh Boundary

## Purpose

Clarify the long-term boundary between detector-owned report production and
`DetectionRuntime` coordination so migration bridges do not become the permanent
architecture.

## Problem

`DetectionRuntime::refreshScalarDetectorReport()` was useful as a scalar
migration bridge, but it would be the wrong long-term pattern if repeated per
detector type.

The architecture must avoid both:

- under-generalizing into one runtime-owned `refreshXXDetectorReport()` function per detector
- over-generalizing too early into a forced `IDetector` plus type-erased feature input

## Detector Genericity Rule

`Detector` is a shared architectural role, not necessarily one forced C++ base
class yet.

The shared outward detector contract is:

- stable `DetectorId` / `DetectorDescriptor`
- accepted `Occurrence` emission
- `DetectorReport` exposure
- selected rejected candidate exposure through `RejectedCandidateSummary`
- generic reject class through `DetectorRejectClass`

The detector-specific internals may remain specialized:

- feature input type
- update method shape
- candidate lifecycle state
- lifecycle implementation
- detector-specific reject reasons
- typed report detail
- typed occurrence detail

Detector-specific detail is allowed inside detector-owned reports.
Detector-specific report assembly in `DetectionRuntime` is migration-only.
`DetectionRuntime` coordinates detectors; it must not become the owner of
detector-specific truth.

## Accepted Long-Term Pattern

```text
detector.update(detector-specific feature input)
detector.pollOccurrence(...)
detector.report()
```

Equivalent detector-local report builders are also acceptable as long as report
production stays detector-owned.

## Rejected Long-Term Pattern

```text
DetectionRuntime::refreshScalarDetectorReport()
DetectionRuntime::refreshFrequencyDetectorReport()
DetectionRuntime::refreshChirpDetectorReport()
DetectionRuntime::refreshKnockDetectorReport()
```

`DetectionRuntime` must not grow one detector-specific report refresh function
per detector type.

## DetectionRuntime Responsibility

`DetectionRuntime` should:

- coordinate profile-selected detectors
- feed detector-specific inputs into detectors
- poll accepted occurrences
- expose detector-owned reports through a generic access path

`DetectionRuntime` should not:

- become the permanent assembler of detector-specific report truth
- duplicate detector-local lifecycle or reject-summary ownership
- force all detectors into the same input/update implementation shape

## What This Means for Frequency Migration

Frequency migration should follow the same outward contract as scalar:

- detector-owned accepted occurrence truth
- detector-owned selected reject truth
- detector-owned typed report detail
- generic `DetectorReport` exposure upward

It does not need to copy the scalar bridge shape into a new
`refreshFrequencyDetectorReport()` function.

## What This Does Not Require Yet

This does not require a forced `IDetector` base class or type-erased feature
input yet.

Different detectors may keep specialized feature inputs and specialized
`update(...)` shapes until the codebase clearly benefits from a tighter shared
interface.
