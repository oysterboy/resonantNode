# Decisions

Status: index of recorded architecture and process decisions.
Scope: forks that were taken deliberately and should not be re-litigated by
accident, plus the ones that are still open.
Purpose: give decisions a home that outlives the pass / plan doc they were
made in. Pass docs under `docs/refactors/` get archived when their work
closes; a decision made inside one is still binding afterwards, and the
next session (or person) needs one predictable place to check "is this
already decided?" before proposing it again.

How this relates to the other docs:

```text
docs/specs/myspec.md        what is always true (the rules)
docs/decisions/             which forks were taken, why, and when to revisit
docs/refactors/             how a decided item gets executed (temporary)
docs/roadmaps/              what is next, in what order
```

A decision is not a rule. When a decision hardens into something the spec
states outright, the spec gets the rule and the decision file stays as the
"why". The decision file is the durable record; the pass doc it came from
is the detailed evidence and may be archived.

## Format

One file per decision, named `YYYY-MM-DD-short-slug.md`, with these
sections (keep each short, usually under 40 lines total):

```text
Status       decided | decided, not yet implemented | superseded | open
Date
Decision     one or two sentences, the fork taken
Why          the reasons, with numbers where they were measured
Rules out    what this closes off, so it isn't re-proposed
Revisit when the condition under which this should be reopened
Source       the doc, section and commit where it was made
```

A superseded decision keeps its file with Status changed and a pointer to
what replaced it.

## Decided

| Date | Decision |
|---|---|
| 2026-09-20 | [Occurrence detail fields are evidence namespaces, not per-detector slots](2026-09-20-occurrence-detail-namespaces.md) |
| 2026-09-21 | [Defer the single-active-detector union; superseded by family build flag](2026-09-21-defer-single-detector-union.md) |
| 2026-09-21 | [Rename PatternMatcher / PatternResult to OccurrenceEvaluator / OccurrenceVerdict](2026-09-21-rename-patternmatcher-to-occurrenceevaluator.md) |
| 2026-09-21 | [Fold OccurrenceEvaluator into OccurrenceInspector (Phase 7c, not yet implemented)](2026-09-21-fold-evaluator-into-inspector.md) |
| 2026-09-23 | [Detector family is a build flag; no virtual detector or runtime interface](2026-09-23-no-virtual-detector-interface.md) |
| 2026-09-23 | [Keep ParamRegistry in this repo as a PlatformIO library](2026-09-23-keep-paramregistry-in-repo.md) |
| 2026-10-06 | [Nodes move to D-AMP output (MAX98357A over I2S); board is a build variant](2026-10-06-damp-output-hardware.md) |
| 2026-10-08 | [Commit cited hardware runs under bench/, with setup and firmware hash](2026-10-08-bench-data-in-repo.md) |

## Open

These are known forks that have not been taken. Each points at where the
question is tracked; a file is added here when one resolves.

| Question | Tracked in |
|---|---|
| Which detection profile the Node ships with (`TonalPulseFreq` vs `TonalPulseScalar`) | DET-007 in `docs/roadmaps/roadmap-detection.md`; step 5 of `roadmap-0-steps.md`, decided on D-AMP |
| Keep or consolidate `FrequencyMatchDetector` (needs the matched-condition field trials) | `docs/refactors/cleanup-0-plan.md` Phase 0; working assumption is "keep" |
| Which metrics belong in the generic `Occurrence` core vs detector-specific detail | `docs/specs/myspec.md` §5 ("temporary typed accepted-event detail"); DET-003..006 |
| How much `FeatureHistory` to retain on-device | `docs/refactors/cleanup-0-plan.md` Phase 5c (33 KB, 80% of `DetectionRuntime` at last measurement) |
| Exact boundary between analyzer-only and runtime diagnostics | `docs/refactors/cleanup-analyzer-node-isolation.md`; Phase 5a landed the first cut |
| Persistent (flash/NVS) config | PAR-010 / PAR-011, deferred until after the field trial (step 11) |
| Keep First Difference as the I2S PCM preprocessor on the D-AMP HAL, or replace it with a DC blocker (retune) | `docs/refactors/i2s-first-difference-revisit.md`; bench tests in issue #24, applied in issue #19 (D-AMP HAL) |
