# Codex Pass - Section E: Pattern Layer

## Landed state

The pattern layer hard split has landed.

Current ownership is:

```text
InspectedSignal
-> PatternAssembler
-> PatternCandidate
-> PatternRules
-> PatternResult
```

The compatibility bridge remains in `DetectionPipeline.h`, but the canonical pattern payload types now live in the pattern headers.

## What is now true

- `PatternCandidate` is explicit in `src/detection/patterns/PatternCandidate.h`
- `PatternResult` is explicit in `src/detection/patterns/PatternResult.h`
- shared pattern enums and evidence payloads live in `src/detection/patterns/PatternTypes.h`
- `DetectionPipeline.h` is now a compatibility shim for legacy / analyzer code
- `PatternAssembler` remains the only stage creating candidates
- `PatternRules` remains the only stage interpreting candidates into meaning
- `Behavior` still consumes `PatternResult + FieldState`

## What remains compatible

- legacy AMP comparison paths
- analyzer SEQ compatibility logging
- helper aliases still present in `DetectionPipeline.h`

## What this is not

- not a `DetectionProfile`
- not a behavior rewrite
- not a full chirp / burst finalization pass
- not a legacy cleanup pass

## Next cleanup

- trim remaining compatibility aliases only where the change is mechanical
- keep the behavior path unchanged
- keep pattern ownership with `PatternAssembler` and `PatternRules`
