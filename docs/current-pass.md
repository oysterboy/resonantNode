# Pass X7 - Scalar Rejected-Lifecycle Parity

Status: implementation pass
Scope: detector + analyzer bridge only
Goal: bring `ScalarTransientDetector` up to the same rejected-lifecycle reporting standard as `FrequencyMatchDetector`.

---

## Target

Compare the scalar and frequency detector paths with one focused question:

```text
Can scalar reporting keep a best rejected pending lifecycle, just like frequency reporting already does?
```

The answer should be yes.

Scalar should track its own best rejected pending lifecycle and expose the result through the existing canonical detector report path.
The scalar rule can stay scalar-specific.

## Scalar reject rule

Use a simple scalar-specific ranking for the best rejected pending lifecycle.

Preferred ordering:
1. Longer rejected duration wins
2. If duration ties, stronger rejected peak wins
3. If still tied, keep the earlier or first stable choice already in the detector

Duration and strength are the likely right rule pair for now.
Do not over-generalize this into frequency rules.

## Check both detectors for parity

Compare:

```text
ScalarTransientDetector
FrequencyMatchDetector
```

Across:

```text
DetectorReport
SelectedRejectSummary
accepted Occurrence emission
selected reject reporting
threshold reporting
aggregate counters
reset behavior
profile/runtime access path
Analyzer bridge input
SEQ_INSPECT / SEQ_TRIAL output support
```

Required questions:

1. Does scalar now track a best rejected pending lifecycle?
2. Does scalar still emit accepted `Occurrence` the same way?
3. Does scalar build `SelectedRejectSummary` from its best rejected lifecycle?
4. Does `DetectionRuntime` only route/snapshot the detector truth?
5. Does Analyzer keep reading only canonical detector report data plus `InspectedOccurrence` payload facts?
6. Are detector-specific fields still cleanly namespaced as `detail.scalar.*` and `detail.frequency.*`?

## Do not broaden scope

Do not change:

- detector thresholds
- detection behavior
- `Occurrence` contract
- `PatternResult` contract
- analyzer output labels

Only make small compile-safe fixes if needed.

## Output doc

Create:

```text
docs/scalar_rejected_lifecycle_parity.md
```

Include:

- `## Scalar path`
- `## FrequencyMatch path`
- `## Best-reject rule`
- `## Parity table`
- `## Remaining asymmetries`
- `## Which asymmetries are acceptable`
- `## Which asymmetries need cleanup`
- `## Recommended next pass`

## Acceptance

- Scalar tracks a best rejected pending lifecycle.
- Scalar selected-reject reporting comes from that lifecycle.
- Frequency and scalar remain canonically comparable through `DetectorReport`.
- Analyzer does not need new non-canonical bridge truth.
- No behavior changes outside the reject snapshot choice.
- Build passes if code is touched.

## Recommended next pass

- audit whether `DetectionPipelineResult` still needs both `_lastOccurrence` and `_lastInspectedOccurrence`
- confirm that analyzer display code can stay on canonical report fields plus `InspectedOccurrence` payload facts
- keep `DetectorReport` as the single detector-stage report contract and avoid rebuilding detector truth in `DetectionRuntime`
