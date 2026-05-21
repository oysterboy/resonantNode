# Pattern File Reorg

## Goal

Clean up the pattern-side file layout so each file has a clear ownership boundary, while keeping the `detection` namespace and current runtime behavior unchanged.

Target split:

- `src/detection/patterns/PatternTypes.h`
- `src/detection/patterns/PatternCandidate.h`
- `src/detection/patterns/PatternResult.h`
- `src/detection/patterns/PatternRules.h/.cpp`
- `src/detection/patterns/PatternAssembler.h/.cpp`
- `src/detection/patterns/PatternNames.h`

Keep:

- the `detection` namespace
- current behavior
- current profile composition

## Current Scope

The inspector-owned evidence split is done. The next cleanup is the pattern-side organization:

- keep `PatternCandidate.h` and `PatternResult.h` as focused shape files
- keep `PatternAssembler` and `PatternRules` as logic files
- slim `PatternTypes.h` so it only holds pattern-owned enums and shared pattern vocabulary
- decide whether `PatternTypes.h` still needs a rename after the trim

### Keep

- `PatternCandidate.h`
- `PatternResult.h`
- `PatternAssembler.h/.cpp`
- `PatternRules.h/.cpp`
- `PatternNames.h`

### Split or Trim

- `PatternTypes.h`

### Possible Rename

- `PatternTypes.h`

## Do Now

1. Trim `PatternTypes.h` to pattern-only enums and vocabulary.
2. Keep `PatternCandidate.h` / `PatternResult.h` as separate focused files.
3. Keep `PatternAssembler` / `PatternRules` separate.
4. Build both `esp32dev` and `esp32dev-analyzer`.

## Do Not Do

- do not change detection behavior
- do not change profile values
- do not add a compatibility layer
- do not leave long-lived forwarding headers behind
- do not change the `detection` namespace

## Why

The current file layout is correct by namespace, but a few pattern headers still feel broader than their ownership. Splitting by ownership keeps the code easy to scan without flattening it into a mega file.

## Status

Done in active source and verified with both `esp32dev` and `esp32dev-analyzer` builds.

Removed the umbrella `PatternPayload.h` layer and switched consumers to the concrete candidate/result headers directly.
