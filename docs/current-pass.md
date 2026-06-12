# Pass X2 - Occurrence Payload Cleanup

Status: Codex instruction  
Scope: active `src/detection` and `src/modes/analyzer` occurrence chain  
Goal: remove the legacy/transitional `Occurrence` payload spillover by moving active consumers onto the canonical occurrence shell plus scoped detail blocks.

---

## Starting point

X1 removed `DetectionDiagnostics`, the legacy Analyzer bridge, BASE/CAP/VAL tooling, and post-DDQ residue.

Remaining UNKNOWN from X1-R:

```text
src/detection/occurrences/Occurrence.h
src/detection/patterns/PatternResult.h
src/detection/detectors/ScalarTransientDetector.cpp
OccurrenceSource usage in runtime/detector/pattern/analyzer logic
```

This pass handles only the `Occurrence` payload. `PatternResult` trimming is a follow-up pass unless a trivial compile-only change becomes necessary.

---

## Current problem

`Occurrence` still contains a canonical shell plus legacy payload:

```cpp
DetectorId detectorId;
OccurrenceType occurrenceType;
bool present;
bool valid;
start/peak/release/end samples and ms;
strength;
confidence;
ScalarOccurrenceDetail scalar;

// legacy / transitional spillover
OccurrenceKind kind;
OccurrenceSource source;
OccurrenceDetectorKind detectorKind;
score;
contrast;
ampStrength;
scalarEvidence;
frequencyScoreStrength;
frequencyContrastQuality;
targetBandStrength;
ampLevel;
ampBaseline;
TransientEvidence transient;
FrequencyBandMeasurementPacket frequency;
```

The active chain still reads old identity fields and old wide payload fields in:

```text
DetectionRuntime
ScalarTransientDetector
FrequencyMatchDetector
OccurrenceInspector
PatternAssembler
AnalyzerApp
AnalyzerReportingTypes
```

---

## Desired end state

`Occurrence` should become a compact accepted-event contract:

```cpp
struct Occurrence {
    DetectorId detectorId;
    OccurrenceType occurrenceType;
    bool present;
    bool valid;

    uint64_t startSample;
    uint64_t peakSample;
    uint64_t releaseSample;

    unsigned long startMs;
    unsigned long peakMs;
    unsigned long releaseMs;
    unsigned long endMs;
    unsigned long durationMs;

    float strength;
    float confidence;

    ScalarOccurrenceDetail scalar;
    FrequencyOccurrenceDetail frequency;
};
```

No active code should read:

```text
OccurrenceKind
OccurrenceSource
OccurrenceDetectorKind
detectorIdFromLegacyOccurrenceSource()
occurrenceTypeFromLegacyOccurrenceKind()
Occurrence::score
Occurrence::contrast
Occurrence::ampStrength
Occurrence::scalarEvidence
Occurrence::frequencyScoreStrength
Occurrence::frequencyContrastQuality
Occurrence::targetBandStrength
Occurrence::ampLevel
Occurrence::ampBaseline
Occurrence::transient
```

Legacy aliases and bridge helpers should be removed from `Occurrence.h` only after all active producers and consumers are migrated.

---

## Guardrails

Do not change:

```text
detector behavior
thresholds
PatternMatcher internals
PatternResult semantics, except compile fixes caused by occurrence field moves
Behavior / Output
clean SEQ output labels
emitted event label strings
timing math
accept/reject decisions
```

Do not remove:

```text
DetectorReport
RejectedCandidateSummary
AnalyzerReport / AnalyzerReportingTypes
RAW/sample dump tooling
SEQ REPORT
SEQ STATUS
SYSTEM_HEALTH
```

If a field is unclear, keep it and classify it as `UNKNOWN`.

---

## Classification labels

Use only these labels in reports:

```text
MOVE_NOW
DELETE_NOW
KEEP_CANONICAL
KEEP_NEUTRAL_TOOLING
ROADMAP_LATER
BUG_RISK
UNKNOWN
```

Meanings:

```text
MOVE_NOW          active payload should move to canonical/scoped detail
DELETE_NOW        dead legacy alias/helper after all reads are gone
KEEP_CANONICAL    part of the desired occurrence contract
KEEP_NEUTRAL_TOOLING output/tooling fact, but not analyzer truth
ROADMAP_LATER     belongs to PatternResult or later report cleanup
BUG_RISK          changing it may affect behavior/timing/output semantics
UNKNOWN           not enough certainty; leave untouched
```

---

## X2-A - Occurrence payload audit

Search:

```text
OccurrenceKind
OccurrenceSource
OccurrenceDetectorKind
detectorIdFromLegacyOccurrenceSource
occurrenceTypeFromLegacyOccurrenceKind
.kind
.source
.detectorKind
.score
.contrast
.ampStrength
.scalarEvidence
.frequencyScoreStrength
.frequencyContrastQuality
.targetBandStrength
.ampLevel
.ampBaseline
.transient
.frequency
```

Scope:

```text
src/detection
src/modes/analyzer
```

Create:

```text
docs/occurrence_payload_cleanup_audit.md
```

Include:

```text
## Active producers
## Active consumers
## MOVE_NOW
## DELETE_NOW
## KEEP_CANONICAL
## KEEP_NEUTRAL_TOOLING
## ROADMAP_LATER
## BUG_RISK
## UNKNOWN
## Proposed migration order
```

Acceptance:

```text
Every active legacy/transitional Occurrence field read/write is classified.
No code changes are required in X2-A except the audit doc.
```

---

## X2-B - Define scoped occurrence details

Extend `Occurrence.h` with explicit scoped detail blocks before moving users:

```cpp
struct ScalarOccurrenceDetail {
    bool present;
    float value;
    float baseline;
    float lift;
    float strength;
    float onsetStrength;
    float peakStrength;
    float releaseStrength;
    bool audioOverflowDuringCandidate;
    ScalarEvidence evidence;
    StrengthClass strengthClass;
};

struct FrequencyOccurrenceDetail {
    bool present;
    float score;
    float contrast;
    StrengthClass scoreStrength;
    StrengthClass contrastQuality;
    StrengthClass targetBandStrength;
    FrequencyBandMeasurementPacket measurement;
};
```

Do not introduce a third `TransientOccurrenceDetail` occurrence family. The old
transient lifecycle payload belongs to scalar occurrence detail because the
active occurrence families are scalar and frequency only.

Rules:

```text
Do not remove old fields in X2-B.
Add new fields beside the existing compatibility payload.
Keep defaults zero/false/Unknown.
Build after adding the structs.
```

Acceptance:

```text
Occurrence has canonical scoped detail blocks for scalar and frequency facts.
Scalar detail includes the old transient lifecycle facts needed by current
pattern/analyzer consumers.
No active behavior/output changes.
Build passes.
```

---

## X2-C - Move producers to canonical/scoped fields

Migrate producers to fill canonical fields first.

Producer targets:

```text
FrequencyMatchDetector::capturePendingOccurrence()
ScalarTransientDetector::capturePendingOccurrence()
DetectionRuntime scalar update call sites
OccurrenceInspector scalar annotation, if it currently backfills legacy fields
```

Producer mapping:

```text
kind/source/detectorKind
    -> detectorId / occurrenceType

score / contrast
    -> occurrence.frequency.score / occurrence.frequency.contrast

frequencyScoreStrength / frequencyContrastQuality / targetBandStrength
    -> occurrence.frequency.scoreStrength / contrastQuality / targetBandStrength

frequency
    -> occurrence.frequency.measurement

ampLevel / ampBaseline / scalarEvidence / ampStrength
    -> occurrence.scalar.value / baseline / evidence / strengthClass

transient
    -> occurrence.scalar onset/peak/release lifecycle fields
```

Rules:

```text
During X2-C, keep writing old fields too if consumers still need them.
Prefer helper fill functions to duplicated assignments if the mapping repeats.
Do not change emitted values.
```

Acceptance:

```text
Canonical/scoped fields are fully populated by occurrence producers.
Old fields may still be written for compatibility.
Build passes.
```

---

## X2-D - Move consumers off legacy fields

Migrate active consumers to read canonical/scoped fields.

Consumer targets:

```text
PatternAssembler
AnalyzerApp occurrence display helpers and report fill
OccurrenceInspector
AnalyzerReportingTypes fields, if they duplicate old names
```

Consumer mapping:

```text
source.kind
    -> source.occurrenceType

source.source
    -> source.detectorId

source.detectorKind
    -> source.detectorId or a scoped detail only if truly needed

source.score / source.contrast
    -> source.frequency.score / source.frequency.contrast

source.ampStrength
    -> source.scalar.strengthClass

source.scalarEvidence
    -> source.scalar.evidence

source.frequencyScoreStrength
    -> source.frequency.scoreStrength

source.frequencyContrastQuality
    -> source.frequency.contrastQuality

source.targetBandStrength
    -> source.frequency.targetBandStrength

source.ampLevel / source.ampBaseline
    -> source.scalar.value / source.scalar.baseline

source.transient
    -> source.scalar onset/peak/release lifecycle fields

source.frequency
    -> source.frequency.measurement
```

Rules:

```text
Do not change output labels unless a label names the removed internal field.
Do not change PatternCandidate semantics.
If PatternAssembler requires old-shaped data, add local adapter helpers rather than keeping legacy fields on Occurrence.
```

Acceptance:

```text
No active consumer reads legacy Occurrence identity or payload aliases.
Build passes.
SEQ outputs retain their labels and meanings.
```

---

## X2-E - Delete legacy Occurrence aliases

Only after X2-D passes, remove from `Occurrence.h`:

```text
OccurrenceKind
OccurrenceSource
OccurrenceDetectorKind
detectorIdFromLegacyOccurrenceSource()
occurrenceTypeFromLegacyOccurrenceKind()
kind
source
detectorKind
score
contrast
ampStrength
scalarEvidence
frequencyScoreStrength
frequencyContrastQuality
targetBandStrength
ampLevel
ampBaseline
legacy/transitional comments
```

Do not remove:

```text
Occurrence::frequency
```

once it is the canonical `FrequencyOccurrenceDetail` block. Remove only the old
packet-shaped frequency payload after all consumers have moved to
`Occurrence::frequency.measurement`.

Create:

```text
docs/occurrence_payload_cleanup_removal.md
```

Include:

```text
## Removed aliases
## Fields kept canonical
## Consumers migrated
## Compile fixes
## Remaining UNKNOWN
## Next recommended pass
```

Acceptance:

```text
No active references remain to deleted aliases/helpers/enums.
Occurrence.h no longer contains legacy/transitional payload comments.
Build passes.
```

---

## Required scans

Before and after X2-E:

```bash
rg -n "OccurrenceKind|OccurrenceSource|OccurrenceDetectorKind|detectorIdFromLegacyOccurrenceSource|occurrenceTypeFromLegacyOccurrenceKind" src/detection src/modes/analyzer
rg -n "\\.(kind|source|detectorKind|score|contrast|ampStrength|scalarEvidence|frequencyScoreStrength|frequencyContrastQuality|targetBandStrength|ampLevel|ampBaseline)\\b" src/detection src/modes/analyzer
rg -n "legacy|Legacy|compat|Compat|temporary|transitional|TODO|FIXME" src/detection/occurrences src/detection/patterns src/modes/analyzer
```

Expected final result:

```text
No legacy Occurrence identity enum/helper references remain.
No old wide Occurrence payload alias reads remain.
Only ROADMAP_LATER PatternResult transitional comments may remain.
```

---

## Build validation

Run after each implementation phase:

```bash
platformio run -e esp32dev-analyzer
platformio run
```

Pass accepted only if both builds pass.

---

## Commit style

Suggested split:

```bash
git commit -m "DetectionCleanup audit occurrence payload"
git commit -m "DetectionCleanup add scoped occurrence details"
git commit -m "DetectionCleanup migrate occurrence payload consumers"
git commit -m "DetectionCleanup remove legacy occurrence payload"
```

If implemented as one pass:

```bash
git commit -m "DetectionCleanup clean occurrence payload"
```
