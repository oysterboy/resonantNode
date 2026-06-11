# Pass H — Route Scalar Occurrence Emission Directly from ScalarTransientDetector

Status: Codex instruction  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Phase: Phase 3 implementation, scalar detector ownership cleanup  
Primary goal: move accepted scalar `Occurrence` emission ownership from `ScalarOccurrenceSource` into `ScalarTransientDetector`, while preserving downstream contracts and behavior

---

## Goal

Move scalar accepted `Occurrence` emission into the canonical detector core:

```text
ScalarTransientDetector
```

Current remaining scalar under-generalization risk:

```text
ScalarOccurrenceSource still builds and drains the emitted scalar Occurrence.
DetectionRuntime still drains through wrapper-era source routing.
```

Target for this pass:

```text
ScalarTransientDetector
  -> poll accepted Occurrence
  -> DetectionRuntime drains Occurrence
  -> Inspector / Pattern path unchanged
```

This is a narrow ownership move.

It must not become a general runtime rewrite.

---

## Required input docs

Read these before editing code:

```text
docs/g2abc_checkpoint_before_pass_h.md
docs/detection_contract_decisions.md
docs/detection_minimal_contracts.md
docs/detection_contract_name_mapping.md
docs/detection_contract_trim_inventory.md
docs/detection_diagnostics_containment.md
docs/detector_report_scalar_path.md
docs/analyzer_scalar_report_bridge.md
docs/scalar_report_detector_core_migration.md
docs/generic_detector_report_refresh_boundary.md
docs/implementation-status.md
docs/roadmaps/roadmap_detection.md
```

If `docs/roadmaps/roadmap_detection.md` does not exist in this checkout, continue without it and note that in the report.

---

## G2a/G2b/G2c checkpoint conclusion

The checkpoint before Pass H concluded:

```text
No hard blocker was found that requires a separate pre-pass first.
```

Recommended Pass H scope:

```text
1. let ScalarTransientDetector own accepted scalar Occurrence emission
2. let DetectionRuntime drain that accepted Occurrence without changing downstream stage contracts
3. keep ScalarOccurrenceSource only as a temporary shell if needed during the move,
   or remove/bypass it if the change is clean and local
4. preserve current Occurrence payload shape
5. preserve current scalar DetectorReport path
6. preserve current analyzer compatibility behavior
```

Pass H must therefore stay focused on:

```text
Detector -> Occurrence ownership
```

not broader architecture cleanup.

---

## Current state to assume

From the G2 checkpoint:

```text
- scalar report ownership is already detector-local
- Analyzer scalar report synthesis consumes scalarDetectorReport()
- DetectionDiagnostics remains a compatibility copy
- scalar accepted Occurrence emission is still wrapper-local
- ScalarOccurrenceSource builds the emitted scalar Occurrence
- ScalarOccurrenceSource::popOccurrence(...) drains it
- DetectionRuntime::drainOccurrenceSources(...) still branches on _occurrenceSourceKind
- Frequency path remains wrapper-owned and legacy
```

---

## Architectural rule to preserve

```text
Generic outward detector contract.
Specialized detector input/update internals.
Detector-owned occurrence emission and report production.
DetectionRuntime coordination only.
```

Allowed:

```text
ScalarTransientDetector may consume scalar feature input.
FrequencyMatchDetector may continue to consume frequency feature input later.
Both should converge on accepted Occurrence emission and DetectorReport exposure.
```

Not allowed as long-term direction:

```text
DetectionRuntime::drainScalarOccurrence()
DetectionRuntime::drainFrequencyOccurrence()
DetectionRuntime::drainChirpOccurrence()
```

If temporary scalar-specific runtime code remains, mark it clearly as migration-only.

---

## Main tasks

### 01. Inspect current scalar occurrence emission

Inspect:

```text
src/detection/detectors/ScalarTransientDetector.h
src/detection/detectors/ScalarTransientDetector.cpp
src/detection/occurrences/ScalarOccurrenceSource.h
src/detection/occurrences/ScalarOccurrenceSource.cpp
src/detection/occurrences/Occurrence.h
src/detection/DetectionRuntime.h
src/detection/DetectionRuntime.cpp
```

Find:

```text
ScalarOccurrenceSource occurrence construction
ScalarOccurrenceSource::popOccurrence(...)
ScalarTransientDetector accepted candidate state
ScalarTransientDetector accepted occurrence/report facts
DetectionRuntime::drainOccurrenceSources(...)
_occurrenceSourceKind scalar/frequency branching
Occurrence payload fields currently set by ScalarOccurrenceSource
```

---

### 02. Add detector-owned accepted Occurrence emission for scalar

Preferred shape:

```cpp
bool ScalarTransientDetector::pollOccurrence(Occurrence& out);
```

or, if naming already differs:

```cpp
bool ScalarTransientDetector::popOccurrence(Occurrence& out);
```

The method should:

```text
- return true only when the scalar detector has an accepted occurrence ready
- populate the same scalar Occurrence payload shape currently emitted by ScalarOccurrenceSource
- clear/consume the pending accepted occurrence once polled
- not expose rejected candidates through Occurrence
- not alter DetectorReport ownership
```

If the detector currently lacks enough state to build the current occurrence payload, move only the minimum accepted-occurrence facts into `ScalarTransientDetector`.

Do not move analyzer/report-only fields into `Occurrence`.

---

### 03. Preserve current Occurrence payload shape

The emitted scalar `Occurrence` should remain behaviorally equivalent to the old wrapper-emitted occurrence.

Preserve currently used accepted-event fields, especially those consumed downstream by:

```text
OccurrenceInspector
PatternAssembler
PatternRules
PatternResult
Analyzer legacy output
```

Do not trim or redesign `Occurrence` in this pass.

Rule:

```text
Occurrence = accepted event facts needed downstream.
DetectorReport = explanation, rejected candidates, thresholds, counters.
```

---

### 04. Update DetectionRuntime scalar drain to use detector-owned occurrence

Change scalar occurrence drain path so the scalar accepted occurrence comes from:

```text
ScalarTransientDetector
```

not from `ScalarOccurrenceSource`.

Possible acceptable outcomes:

```text
Option A:
  DetectionRuntime directly polls ScalarTransientDetector for scalar occurrence.

Option B:
  ScalarOccurrenceSource remains as a temporary shell, but delegates occurrence polling
  to ScalarTransientDetector and no longer owns occurrence construction.

Option C:
  If direct drain is unsafe, keep the wrapper temporarily but document exact blockers
  and reduce ownership as far as safe.
```

Preferred outcome:

```text
ScalarOccurrenceSource no longer owns scalar accepted Occurrence construction.
```

If `ScalarOccurrenceSource` remains, it should be clearly marked:

```text
temporary compatibility shell
```

---

### 05. Keep downstream stages unchanged

Do not change:

```text
OccurrenceInspector
InspectedOccurrence
PatternAssembler
PatternRules
PatternResult
AnalyzerApp
AnalyzerLegacyReporting
Behavior
```

If any downstream file must be touched only because of include/signature fallout, keep the change mechanical and document it.

If H seems to require pattern-stage changes, stop and report that the pass scope has widened too far.

---

### 06. Keep scalar DetectorReport path intact

Do not break:

```text
ScalarTransientDetector::buildReport(...)
DetectionRuntime::scalarDetectorReport()
Analyzer scalar bridge from Pass E
DetectionDiagnostics compatibility copy
```

If the accepted occurrence source changes, make sure scalar report accepted-present/timing facts still match the emitted occurrence.

Do not move report fields back into `DetectionRuntime`.

---

### 07. Leave FrequencyMatch untouched

Do not migrate:

```text
FrequencyOccurrenceSource
FrequencyMatchDetector occurrence emission
frequency DetectorReport
frequency Analyzer reporting
frequency runtime routing
```

Frequency remains the later parity/migration target.

This pass should establish a scalar pattern that frequency can later follow, not migrate both at once.

---

### 08. Leave OccurrenceSourceKind routing in place unless trivial

`OccurrenceSourceKind` is still legacy routing vocabulary.

For this pass:

```text
- do not redesign OccurrenceSourceKind
- do not rename profile routing
- do not split DetectorId / DetectorRole / DetectorSelection
```

It is acceptable for `DetectionRuntime` to keep `_occurrenceSourceKind` branching temporarily.

But do not deepen `OccurrenceSourceKind` into new long-term detector identity responsibilities.

---

### 09. Update docs

Create or update:

```text
docs/scalar_occurrence_emission_migration.md
```

Required sections:

```text
# Scalar Occurrence Emission Migration

## Purpose

## Previous Scalar Occurrence Path

## New Scalar Occurrence Path

## ScalarTransientDetector Ownership

## ScalarOccurrenceSource Status

## DetectionRuntime Drain Path

## Occurrence Payload Compatibility

## DetectorReport Compatibility

## Analyzer / Pattern Compatibility

## Frequency Path Status

## What Did Not Change

## Remaining Temporary Bridges

## Recommended Next Pass
```

Also update, if meaningful:

```text
docs/g2abc_checkpoint_before_pass_h.md
docs/scalar_report_detector_core_migration.md
docs/generic_detector_report_refresh_boundary.md
docs/implementation-status.md
docs/current-pass.md
```

Do not duplicate long explanations everywhere.

---

## Allowed code changes

Allowed:

```text
- add ScalarTransientDetector::pollOccurrence(...) or equivalent
- move scalar accepted occurrence construction from ScalarOccurrenceSource to ScalarTransientDetector
- make ScalarOccurrenceSource delegate to ScalarTransientDetector, if kept as a temporary shell
- update DetectionRuntime scalar drain path
- update local includes/signatures needed for the move
- update docs
```

Not allowed:

```text
- FrequencyMatch migration
- generic DetectorReport access redesign
- OccurrenceSourceKind model redesign
- DetectionDiagnostics deletion
- Analyzer output redesign
- Occurrence payload trimming
- PatternAssembler / PatternRules cleanup
- PatternResult cleanup
- threshold/profile/timing tuning
- forced IDetector / type-erased feature input
- behavior changes
```

---

## Compile and test checkpoint

Run:

```text
platformio run -e esp32dev-analyzer
```

Expected:

```text
success
```

Runtime behavior change:

```text
expected none
```

If possible, run a short scalar-oriented SEQ sanity check.

Minimum recommended runtime sanity:

```text
- one Amp / scalar profile short run if available
- confirm accepted scalar occurrence still reaches Inspector / Pattern path
- confirm scalar DetectorReport still populates
- confirm legacy Analyzer output still prints
```

If no runtime test is run, state that clearly.

---

## Expected output report

After completing this pass, report:

```text
Files created
Files updated
Where scalar Occurrence construction lived before
Where scalar Occurrence construction lives after
Whether ScalarTransientDetector now exposes pollOccurrence/popOccurrence
Whether ScalarOccurrenceSource remains
If ScalarOccurrenceSource remains, whether it is shell/delegating/legacy-only
Whether DetectionRuntime scalar drain changed
Whether Occurrence payload shape changed
Whether DetectorReport path changed
Whether Analyzer output changed
Whether Pattern stage changed
Whether Frequency path changed
Path of docs/scalar_occurrence_emission_migration.md
Compile result
Runtime sanity result if run
Runtime behavior change: expected none
Remaining risks
Recommended next pass
```

---

## Acceptance criteria

This pass is accepted if:

```text
- accepted scalar Occurrence emission is owned by ScalarTransientDetector,
  or ScalarOccurrenceSource is reduced to a clearly temporary delegating shell
- DetectionRuntime drains scalar accepted Occurrence without relying on wrapper-owned construction
- current Occurrence payload shape is preserved
- Inspector / Pattern / Analyzer behavior remains stable
- scalar DetectorReport path remains stable
- Frequency path remains untouched
- no threshold/profile/timing tuning occurred
- build succeeds
```

---

## Recommended next pass

Recommended next pass depends on the result.

If `ScalarOccurrenceSource` is fully bypassed or only a shell:

```text
Pass I — Begin FrequencyMatch DetectorReport Migration
```

Purpose:

```text
Bring FrequencyMatch into the same DetectorReport / occurrence-emission ownership direction,
using the scalar path as reference.
```

If scalar wrapper still has meaningful runtime responsibilities:

```text
Pass H2 — Remove Remaining ScalarOccurrenceSource Runtime Responsibilities
```

If generic report access becomes the immediate blocker before frequency migration:

```text
Pass H2 — Add Generic DetectorReport Access Without Migrating Frequency
```
