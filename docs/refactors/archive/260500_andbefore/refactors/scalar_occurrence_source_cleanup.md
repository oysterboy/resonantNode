# ScalarOccurrenceSource Cleanup

## Purpose

Record Pass H2, which removes the remaining scalar runtime responsibilities
from `ScalarOccurrenceSource` and leaves scalar runtime ownership in
`ScalarTransientDetector`.

## Post-H Starting State

After Pass H:

- accepted scalar `Occurrence` emission was already detector-owned
- canonical scalar `DetectorReport` assembly was already detector-owned
- `ScalarOccurrenceSource` still owned legacy scalar reject-summary aggregates
- `ScalarOccurrenceSource` still owned wrapper-era scalar candidate bookkeeping
- `DetectionRuntime` still called scalar wrapper getters for diagnostics

## Responsibilities Found

Remaining wrapper-owned groups found before H2:

- `DELETE_IF_UNUSED`: accepted scalar `Occurrence` polling shell
- `DELETE_IF_UNUSED`: scalar observation/config/reset forwarding
- `MOVE_TO_SCALAR_TRANSIENT_DETECTOR`: legacy scalar reject-summary aggregates
- `MOVE_TO_SCALAR_TRANSIENT_DETECTOR`: temporary scalar compatibility facts
- `MOVE_TO_SCALAR_TRANSIENT_DETECTOR`: scalar rejected-duration / strength facts
- `DELETE_IF_UNUSED`: scalar last-candidate compatibility getter surface
- `DELETE_IF_UNUSED`: scalar lifecycle fallback getter surface

## Responsibilities Moved

Moved into `ScalarTransientDetector`:

- legacy scalar rejected-candidate aggregate summary
- rejected-candidate count / best duration / second-best duration
- rejected gap totals / max gap / island count
- max rejected peak strength aggregates
- scalar rejected duration / strength compatibility reads

Moved into direct `DetectionRuntime` detector wiring:

- scalar update path now calls `ScalarTransientDetector::update(...)`
- scalar drain path now calls `ScalarTransientDetector::popOccurrence(...)`
- scalar config/reset flow now targets `ScalarTransientDetector` directly
- scalar diagnostics now read detector-owned report data or detector-owned
  temporary legacy summary data

## Responsibilities Deleted

Deleted in H2:

- `src/detection/occurrences/ScalarOccurrenceSource.h`
- `src/detection/occurrences/ScalarOccurrenceSource.cpp`
- wrapper-owned scalar candidate bookkeeping
- wrapper-owned scalar reject-summary state
- wrapper-owned scalar diagnostics getter surface
- wrapper-owned scalar config / observe / pop bridge

## Responsibilities Still Temporary

Temporary compatibility still remains, but no longer in a wrapper:

- `DetectionRuntime::scalarDetectorReport()` is still a scalar-specific accessor
- `DetectionDiagnostics` still carries shared legacy Analyzer compatibility data
- `ScalarTransientDetector::LegacyRejectSummaryCompat` still exists only to feed
  legacy `DetectionDiagnostics` / Analyzer scalar aggregate output

## ScalarTransientDetector Ownership After H2

`ScalarTransientDetector` now owns:

- scalar candidate lifecycle implementation
- accepted scalar `Occurrence` construction and polling
- canonical scalar `DetectorReport` assembly
- canonical selected rejected candidate summary
- temporary scalar legacy reject-summary compatibility aggregates

## DetectionRuntime Dependencies After H2

`DetectionRuntime` now depends on:

- `ScalarTransientDetector::update(...)`
- `ScalarTransientDetector::popOccurrence(...)`
- `ScalarTransientDetector::buildReport(...)`
- `ScalarTransientDetector::reportDetail()`
- `ScalarTransientDetector::legacyRejectSummary()`
- scalar detector reset / summary-reset methods

`DetectionRuntime` no longer calls `ScalarOccurrenceSource`.

## DetectionDiagnostics Compatibility After H2

Scalar compatibility fields now come from detector-owned sources:

- scalar lifecycle and reject labels: detector-built `DetectorReport`
- scalar rejected duration / strength: detector-owned transient reject facts
- scalar source summary aggregates: detector-owned temporary legacy summary
- scalar last-candidate compatibility snapshot: detector report selected reject
  or detector report detail

`DetectionDiagnostics` no longer depends on `ScalarOccurrenceSource`.

## ScalarOccurrenceSource Status

`ScalarOccurrenceSource` was deleted from `src/`.

## Why ScalarOccurrenceSource Was / Was Not Deleted

It was safe to delete because:

- scalar observation/config plumbing no longer needed a wrapper
- accepted scalar `Occurrence` emission was already detector-owned
- canonical scalar report truth was already detector-owned
- scalar diagnostics compatibility data can now be sourced from the detector
- no remaining `src/` code path referenced the wrapper after direct detector
  wiring landed

## Analyzer Compatibility

Analyzer compatibility remains intact:

- Analyzer still reads `DetectionRuntime::scalarDetectorReport()`
- legacy scalar aggregate fallback still flows through `DetectionDiagnostics`
- no Analyzer struct or output naming changed in this pass

## Frequency Path Status

Frequency is unchanged:

- `FrequencyOccurrenceSource` still exists
- frequency occurrence emission is still wrapper-owned
- frequency still lacks detector-owned `DetectorReport` parity

## What Did Not Change

H2 did not:

- tune thresholds, timing, or profiles
- redesign Analyzer output
- remove `DetectionDiagnostics`
- migrate frequency
- add generic detector report access
- change scalar `Occurrence` payload shape
- change pattern-stage behavior

## Remaining Blockers

Remaining cleanup blockers are no longer wrapper blockers:

- scalar report access is still exposed through scalar-specific runtime naming
- detector-owned legacy aggregate compatibility data still needs a later home or
  deletion once Analyzer legacy output no longer needs it
- frequency still needs parity work before shared detector boundaries can settle

## Recommended Next Pass

Recommended next pass:

- `Pass I - Begin FrequencyMatch DetectorReport Migration`
