# Current Pass — H5.1 DetectorReport Snapshot Sections for Scalar

## Goal

Introduce the sectioned `DetectorReport` snapshot model now, using the scalar detector path as the first real implementation.

This pass clarifies the ownership boundary between:

```txt
Detector core → DetectorReport snapshot → Analyzer / Writer
```

It does not migrate the frequency path yet.

## Context

The payload split audit found that the intended boundary is only partly true:

* `OccurrenceDetailKind` is gone from active code.
* `OccurrenceType` is now lean.
* Scalar already has an active `DetectorReport` path.
* Frequency still routes diagnostics through legacy `DetectionDiagnostics` / direct detector access.
* `Occurrence` is accepted-event-only in spirit, but still too wide.
* Pattern candidate cleanup is still needed later, but is out of scope here.

H5 focuses only on the detector/report boundary.

## Occurrence boundary

`Occurrence` is the compact accepted public event.

It should contain:

* generic accepted-event shell:

  * `detectorId`
  * `occurrenceType`
  * start / peak / end timing
  * duration
  * strength
  * confidence if currently available / meaningful
* compact accepted-event detail that is still detector-applicable in shape

For scalar first, prefer a generic scalar detail shape, not AMP-specific naming:

```cpp
struct ScalarOccurrenceDetail {
    bool present = false;
    float value = 0.0f;
    float baseline = 0.0f;
    float lift = 0.0f;
    float strength = 0.0f;
};
```

Important:

* do not call this `AmpTransientDetail`
* do not encode AMP as the occurrence type
* the same scalar detail shape should later be usable for AMP envelope,
  frequency score, frequency contrast, or another scalar carrier
* carrier identity/config may be documented separately, but do not solve full
  `FeatureKind` plumbing in this pass unless already trivial

`Occurrence` should not contain:

* selected reject
* reject aggregates
* thresholds
* frame counters
* detector debug state
* full candidate lifecycle history

## Ownership rule

### Detector core

The detector owns:

* live candidate lifecycle
* raw detector evidence
* reject decisions
* detector-specific aggregates and counters
* mapping from private detector state into reportable facts
* construction of the report snapshot

The detector must not expose raw mutable detector truth and expect Analyzer / Writer to reconstruct meaning from it.

### DetectorReport

`DetectorReport` is a frozen snapshot for one detector / one observation window.

It carries reportable truth only.

It may contain:

* generic top-level report sections
* detector-specific detail blocks

It must not expose raw mutable detector state.

### Analyzer / Writer

Analyzer and SEQ writers only format and classify from the report snapshot.

They must not:

* reconstruct detector truth from private detector fields
* become the owner of detector-specific logic
* duplicate scalar/frequency gate interpretation outside the detector

## Target top-level report model

Keep `DetectorReport` role-based and generic:

```cpp
struct DetectorReport {
    DetectorId detectorId = DetectorId::Unknown;

    unsigned long reportStartMs = 0;
    unsigned long reportEndMs = 0;

    AcceptedOccurrenceSummary accepted = {};
    SelectedRejectSummary selectedReject = {};
    ThresholdSummary thresholds = {};
    AggregateCountSummary aggregates = {};

    ScalarDetectorReportDetail scalar = {};
    FrequencyMatchDetectorReportDetail frequency = {};
};
```

The top-level sections are shared shells only.

## Generic top-level sections

### `AcceptedOccurrenceSummary`

Generic accepted-event truth only:

```cpp
struct AcceptedOccurrenceSummary {
    bool present = false;

    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long endMs = 0;
    unsigned int durationMs = 0;

    float strength = 0.0f;
    float confidence = 0.0f;
};
```

Only include fields another detector could plausibly provide with the same meaning.

### `SelectedRejectSummary`

Generic selected-reject truth only:

```cpp
struct SelectedRejectSummary {
    bool present = false;

    DetectorRejectClass rejectClass = DetectorRejectClass::None;
    uint8_t detectorReason = 0;

    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long endMs = 0;
    unsigned int durationMs = 0;

    float strength = 0.0f;
    float confidence = 0.0f;
};
```

`detectorReason` may hold the detector-specific enum value, but interpretation belongs to detector-specific detail / writer dispatch.

Do not put scalar/frequency-only gate facts here.

### `ThresholdSummary`

Only shared threshold concepts:

```cpp
struct ThresholdSummary {
    unsigned int minDurationMs = 0;
    unsigned int maxDurationMs = 0;
};
```

Do not add detector-private tuning jargon here unless it is truly cross-detector.

### `AggregateCountSummary`

Only shared counts:

```cpp
struct AggregateCountSummary {
    unsigned int acceptedCount = 0;
    unsigned int rejectedCount = 0;
};
```

Detector-specific reject category counts go into the detector-specific detail block.

## Scalar detail block

Scalar-specific payload goes under `report.scalar.*`.

Use carrier-agnostic scalar naming. Do not use AMP-only names for the public report contract.

```cpp
struct ScalarDetectorReportDetail {
    ScalarAcceptedDetail accepted = {};
    ScalarSelectedRejectDetail selectedReject = {};
    ScalarThresholdDetail thresholds = {};
    ScalarAggregateDetail aggregates = {};
    ScalarInspectEvidence inspect = {};
};
```

Conceptual split:

```txt
report.accepted              = generic accepted shell
report.scalar.accepted       = scalar-specific accepted payload

report.selectedReject        = generic selected reject shell
report.scalar.selectedReject = scalar-specific selected reject payload

report.thresholds            = generic shared thresholds
report.scalar.thresholds     = scalar-specific thresholds

report.aggregates            = generic shared counts
report.scalar.aggregates     = scalar-specific aggregate facts
```

## Suggested scalar detail shapes

Use these names or equivalent existing names if less disruptive.

### `ScalarAcceptedDetail`

```cpp
struct ScalarAcceptedDetail {
    bool present = false;

    float value = 0.0f;
    float baseline = 0.0f;
    float lift = 0.0f;
    float normalized = 0.0f;
};
```

This must stay carrier-agnostic.

It should later be usable for:

* AMP envelope
* frequency score
* frequency contrast
* another scalar carrier

### `ScalarSelectedRejectDetail`

```cpp
struct ScalarSelectedRejectDetail {
    bool present = false;

    float value = 0.0f;
    float baseline = 0.0f;
    float lift = 0.0f;
    float normalized = 0.0f;

    bool opened = false;
    bool crossedOnset = false;
    bool crossedRelease = false;
};
```

Only include scalar-specific facts needed for Analyzer / SEQ_INSPECT / SEQ_EXPLAIN.

### `ScalarThresholdDetail`

```cpp
struct ScalarThresholdDetail {
    float onsetThreshold = 0.0f;
    float releaseThreshold = 0.0f;
    float minStrength = 0.0f;
};
```

### `ScalarAggregateDetail`

```cpp
struct ScalarAggregateDetail {
    unsigned int tooShortCount = 0;
    unsigned int tooLongCount = 0;
    unsigned int strengthTooLowCount = 0;

    float maxRejectedLift = 0.0f;
    float bestRejectedValue = 0.0f;
};
```

### `ScalarInspectEvidence`

Only add or keep this if the scalar detector/report currently owns meaningful inspect-like facts.

Keep it bounded and report-facing. Do not use it as a raw mutable detector dump.

## Field placement rule

For every new field, ask:

```txt
Would another detector plausibly have this with the same meaning?
```

If yes, place it in the generic top-level section.

If no, place it in the detector-specific detail block:

```txt
report.scalar.*
report.frequency.*
```

## Runtime rule

`DetectionRuntime` may cache and route reports.

`DetectionRuntime` must not assemble scalar-specific report truth from scattered detector internals.

Preferred direction:

```cpp
DetectorReport report = _scalarDetector.buildReport(...);
```

or:

```cpp
_scalarDetector.buildReport(report);
```

The scalar detector owns the mapping from private detector state to report snapshot.

## Analyzer / Writer rule

Analyzer and writers must read scalar detector truth from `DetectorReport`.

Writing model:

```txt
SEQ_TRIAL
  reads mostly generic top-level fields

SEQ_INSPECT
  reads generic fields first
  then dispatches on detectorId
  then prints report.scalar.*

SEQ_EXPLAIN
  same dispatch model
  may print deeper typed scalar detail
```

Do not add new direct reads from private scalar detector state for detector truth.

## Frequency note

Keep `FrequencyMatchDetectorReportDetail` as an empty or placeholder block if needed for shape parity.

Do not migrate the frequency path in H5.

Later pass:

```txt
Move FrequencyMatchDetector to the same Detector / DetectorReport snapshot model.
Then delete legacy frequency diagnostics and wrapper routing.
```

## In scope

* `DetectorReport` sectioned shape
* `Occurrence` contract clarity for generic shell vs detector-specific detail
* scalar report detail split
* scalar detector-owned report building
* scalar Analyzer / Writer reads from `DetectorReport`
* docs / contract updates for report ownership

## Out of scope

* `PatternCandidate`
* `PatternResult.candidate`
* Behavior cleanup
* FrequencyMatch migration
* deleting `DetectionDiagnostics`
* deleting `FrequencyOccurrenceSource`
* broad legacy naming sweeps
* `Occurrence` trimming beyond what is required for scalar report clarity

## Implementation requirements

* inspect the current scalar `Occurrence` construction
* identify scalar accepted-event fields that are generic enough to keep on
  `Occurrence`
* identify scalar diagnostic / reject / debug fields that should stay only in
  `DetectorReport`
* rename or reshape scalar accepted-event detail away from AMP-specific public
  naming where low-risk
* keep the scalar path compiling and behavior unchanged
* do not attempt to migrate `FrequencyMatch` yet
* do not remove legacy fields if that would trigger broad runtime migration;
  instead mark them as legacy/transitional in comments/docs

## Required docs updates

Update at minimum:

* `docs/detection_payload_split_audit.md`
* `docs/detection_minimal_contracts.md`
* `docs/roadmaps/roadmap_detection.md`
* `docs/current-pass.md`

Docs must state:

```txt
Detector owns truth.
DetectorReport freezes truth.
Analyzer / Writer formats truth.
Scalar is the first implemented reference.
FrequencyMatch migration is later.
PatternCandidate cleanup is out of scope for H5.
```

## Acceptance criteria

* `DetectorReport` has generic top-level role sections:

  * `accepted`
  * `selectedReject`
  * `thresholds`
  * `aggregates`
* scalar-specific report facts live under `report.scalar.*`
* scalar accepted/reject details use carrier-agnostic scalar names, not AMP-only names
* scalar detector owns mapping from private state into the report snapshot
* Analyzer / Writer reads scalar detector truth from `DetectorReport`, not scattered private detector fields
* `DetectionRuntime` only coordinates/caches report snapshots and does not become the detector-specific report assembler
* frequency migration is documented as later
* PatternCandidate / Behavior cleanup is untouched
* `OccurrenceDetailKind` remains absent
* `OccurrenceType` remains lean:

  * `None`
  * `Transient`
  * `FrequencyMatch`
* scalar emitted occurrence uses `OccurrenceType::Transient`, not an AMP-only
  canonical occurrence type
* compile after the change
* report touched files and remaining legacy/transitional report fields
