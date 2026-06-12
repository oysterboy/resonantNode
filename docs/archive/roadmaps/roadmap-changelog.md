# Roadmap Changelog

Status: archive log.
Scope: landed roadmap items moved out of the active roadmaps.
Purpose: keep the active roadmaps future-focused while preserving the landed
history in one place.

---

## Detection / Analyzer

```text
[LANDED] DetectionRuntime coordinates the current detection chain.
[LANDED] DetectorReport is the active detector-stage report contract.
[LANDED] RejectedCandidateSummary is the selected reject contract.
[LANDED] PatternMatcher is the public pattern-stage boundary.
[LANDED] PatternMatcherReport is available for compact pattern facts.
[LANDED] OccurrenceInspector is available for inspection evidence.
[LANDED] FieldStateTracker is available for acoustic context reporting.
[LANDED] Clean analyzer reporting exists with SEQ_TRIAL / SEQ_SOURCE / SEQ_INSPECT / SEQ_EXPLAIN / SEQ_SUMMARY.
[LANDED] DetectionDiagnostics and analyzer legacy compatibility were removed from src.
[LANDED] Analyzer app/reporting exists.
```

## Behavior / Output

```text
[LANDED] ResonantBehavior owns the current behavior decision path.
[LANDED] ChirpOutput remains the current output path.
[LANDED] Behavior consumes PatternResult and FieldState.
[LANDED] ResonantBehavior exists.
```

## Params / Config

```text
[LANDED] Hardcoded DetectionProfile defaults exist.
[LANDED] BehaviorGateConfig defaults exist.
[LANDED] DetectionProfile exists.
```

## Node / Runtime

```text
[LANDED] FieldStateTracker exists.
[LANDED] ChirpOutput / current output path exists.
```

## Procedure

```text
When an active roadmap item is fully landed, move the concise landed note
here and keep the active roadmap focused on the remaining work.
```
