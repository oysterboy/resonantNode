# Roadmap Changelog

Status: archive log.
Scope: landed roadmap items moved out of the active roadmaps.
Purpose: keep the active roadmaps future-focused while preserving the landed
history in one place.

---

## Detection / Analyzer

```text
[LANDED] Clean analyzer reporting exists with SEQ_TRIAL / SEQ_SOURCE / SEQ_INSPECT / SEQ_EXPLAIN / SEQ_SUMMARY.
[LANDED] DetectorReport is the active detector-stage report contract.
[LANDED] PatternMatcher is the public pattern-stage boundary.
[LANDED] FieldStateTracker is available for acoustic context reporting.
```

## Behavior / Output

```text
[LANDED] ResonantBehavior owns the current behavior decision path.
[LANDED] ChirpOutput remains the current output path.
[LANDED] Behavior consumes PatternResult and FieldState.
```

## Params / Config

```text
[LANDED] Hardcoded DetectionProfile defaults exist.
[LANDED] BehaviorGateConfig defaults exist.
```

## Node / Runtime

```text
[LANDED] DetectionRuntime coordinates the current detection chain.
[LANDED] Analyzer app/reporting exists.
```

## Procedure

```text
When an active roadmap item is fully landed, move the concise landed note
here and keep the active roadmap focused on the remaining work.
```
