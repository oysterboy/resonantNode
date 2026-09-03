# Analyzer Trial Truth Canonical Inputs

## Purpose

Move Analyzer trial truth toward frozen canonical inputs:

- expected window
- `DetectorReport`
- `PatternResult`
- `FieldState`

Keep `AnalyzerClassification` generic and keep detector-specific,
occurrence-specific, and pattern-specific reasons on their owning contracts.

## Previous Trial Truth Inputs

Before this pass, Analyzer trial reporting still leaned on a mixed bridge:

- sequence-session bookkeeping such as `primaryValidPatternCaptured`
- `latestPipelineResult()` as a fallback report source
- analyzer-local fallback reason text when no trial `PatternResult` existed
- `DetectionDiagnostics` and legacy source bundles for richer legacy-only output

That shape made canonical inspect/explain vulnerable to reporting facts that were
not actually frozen by the detector or pattern layers for the finalized trial.

## New Trial Truth Inputs

After this pass, Analyzer trial reporting prefers:

- the accepted per-trial `PatternResult` snapshot captured by sequence session
- otherwise the first rejected in-window `PatternResult` snapshot
- the active detector snapshot via `DetectorReport`
- the expected trial window

The clean analyzer path now treats those frozen trial snapshots as canonical.

If a fact was not produced by detector or pattern contracts for the finalized
trial, it is not part of the clean canonical inspect/explain path.

## PatternResult Role

`PatternResult` now owns the canonical pattern-side truth used by Analyzer for
the finalized trial snapshot:

- `pattern.type`
- `pattern.accepted`
- `pattern.candidateAccepted`
- `pattern.patternMatched`
- `pattern.supportMatched`
- `pattern.confidence`
- `pattern.reason`
- `pattern.rejectReason`
- `pattern.involvedOccurrences`
- selected inspected occurrence payload

Important rule enforced in this pass:

- if no finalized-trial `PatternResult` exists, Analyzer does not synthesize a
  fake canonical pattern reason for clean `SEQ_INSPECT` / `SEQ_EXPLAIN`

## DetectorReport Role

`DetectorReport` remains the detector-stage canonical truth surface.

Analyzer now uses detector report availability and detector accepted/reject
presence to keep generic trial classification anchored to detector truth when a
trial-specific `PatternResult` is absent.

Generic Analyzer classification stays small:

- final result
- generic reason
- generic primary stage
- generic `dtMs`

Detector-specific detail still belongs in:

- `reject.class`
- `reject.detector_reason`
- `detail.scalar.*`
- `detail.frequency.*`

## Expected Window Role

The expected window stays Analyzer-owned trial context:

- `expected.triggerMs`
- `expected.windowStartMs`
- `expected.windowEndMs`

When a finalized-trial `PatternResult` exists, Analyzer now derives generic
timing from that selected pattern snapshot instead of relying only on the
session-finalize parameter bridge.

## DetectionDiagnostics Remaining Uses

`DetectionDiagnostics` still remains for legacy compatibility only, mainly for:

- rich legacy source dumps
- legacy frequency fallback fields outside `DetectorReport`
- legacy system/source explain output

It is no longer the model for clean canonical inspect/explain truth.

## Legacy Output Compatibility

Legacy output remains intact through the existing compatibility structs.

The boundary is now explicit:

- canonical `SEQ_INSPECT` / `SEQ_EXPLAIN` may print only contract-produced
  detector/pattern trial facts
- synthesized analyzer fallbacks stay on legacy paths only

## What Did Not Change

- no threshold or timing tuning
- no `Occurrence` payload trim
- no `PatternResult` payload trim
- no detector report field explosion
- no frequency occurrence-emission migration
- no `DetectionDiagnostics` deletion

## Remaining Gaps

- `AnalyzerStage` naming is still legacy-shaped and does not yet mirror the
  cleaner detector / occurrence / inspection / pattern / field split
- `AnalyzerReason` still uses legacy labels such as
  `OccurrenceSeenButRejected` / `InspectionFailed` instead of the cleaner
  generic wording proposed for later cleanup
- pattern-family-specific canonical detail is not yet split into a dedicated
  `detail.pattern.*` namespace
- field-state-driven trial classification is still only partially canonical

## Recommended Next Pass

Recommended next pass: `Pass M - Frequency occurrence emission migration`.
