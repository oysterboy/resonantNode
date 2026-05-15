# Pattern Payload Hard-Split Stability Checklist

Version: v0.3-E-stability-check  
Scope: Decide when `PatternCandidate` and `PatternResult` can be moved out of `DetectionPipeline.h`.

---

## Meaning of “stable” in this scope

Stable does **not** mean the full detection architecture is finished.

Stable means:

```text
The new pattern path is the real path,
and moving pattern payload types will be a type/module cleanup,
not a hidden architecture rewrite.
```

---

## Stable enough checklist

### 1. Runtime path is stable

The main Resonant detection path is:

```text
DetectionRuntime
→ SignalInspector
→ InspectedSignal
→ PatternAssembler
→ PatternRules
→ PatternResult
→ Behavior
```

Checklist:

- [ ] This path compiles.
- [ ] This path runs on the current node setup.
- [ ] This path is the main Resonant behavior path.
- [ ] Legacy AMP is not required for normal behavior triggering.

---

### 2. PatternAssembler ownership is stable

Only `PatternAssembler` creates `PatternCandidate`.

Checklist:

- [ ] `SignalInspector` does not create `PatternCandidate`.
- [ ] `SignalEmitter` does not create `PatternCandidate`.
- [ ] `FrequencyMatchDetector` does not create `PatternCandidate`.
- [ ] Behavior does not create `PatternCandidate`.
- [ ] `PatternAssembler` is the clear owner of pattern candidate creation.

---

### 3. PatternRules ownership is stable

Only `PatternRules` interprets `PatternCandidate` into `PatternResult`.

Checklist:

- [ ] `PatternRules` creates / returns `PatternResult`.
- [ ] `SignalInspector` does not emit final pattern meaning.
- [ ] `SignalEmitter` does not emit final pattern meaning.
- [ ] `FrequencyMatchDetector` does not emit final pattern meaning.
- [ ] Behavior does not classify raw detection facts into pattern meaning.

---

### 4. Current behavior still works

The current useful detection behavior must survive before the hard split.

Checklist:

- [ ] Frequency-first detection still produces results.
- [ ] Behavior still reacts to `PatternResult`.
- [ ] Single-signal pulse path still works.
- [ ] AMP locality still appears if already implemented.
- [ ] Logs still show useful pattern results.
- [ ] No major runtime regression is visible in a short smoke test.

---

### 5. Analyzer / logs are not deeply dependent on old payload shape

Analyzer and debug output should either consume the new pattern result or have a clear adapter path.

Checklist:

- [ ] Analyzer can log `PatternResult` from the new path.
- [ ] Debug logs can show `SIGNAL → INSPECTED → PATTERN_CANDIDATE → PATTERN_RESULT`.
- [ ] If old `DetectionPipeline::PatternResult` is still needed, the dependency is isolated.
- [ ] No analyzer/debug code requires `DetectionPipeline.h` to own the canonical pattern payload types.

---

### 6. Compatibility layer is isolated

If `DetectionPipeline.h` still exists, it should act as a bridge or legacy wrapper, not the conceptual owner of pattern-layer types.

Checklist:

- [ ] `DetectionPipeline.h` is not the only place defining pattern-layer meaning.
- [ ] New code can include pattern-layer types from the pattern module.
- [ ] Any legacy aliases or adapters are clearly marked transitional.
- [ ] Compatibility code does not drive new architecture decisions.

---

### 7. No active logic refactor is bundled into the hard split

The hard split should not happen at the same time as major behavior or detection logic changes.

Do **not** combine the split with:

- [ ] New chirp assembly
- [ ] New behavior rules
- [ ] New FieldState logic
- [ ] DetectionProfile factories
- [ ] FrequencyMatchDetector rewrite
- [ ] Legacy AMP removal
- [ ] Threshold tuning

The split should be a narrow type/module cleanup.

---

## Not required before the split

These are **not** required for “stable enough”:

- [ ] Full chirp grouping
- [ ] DetectionProfile
- [ ] White-noise chain
- [ ] WoodBlock / object detection
- [ ] Final FieldStateConfig
- [ ] All legacy code deleted
- [ ] Perfect final `PatternCandidate` structure

---

## Practical gate

Proceed with the hard split when all are true:

- [ ] Build succeeds.
- [ ] One frequency-first run produces the full stage log sequence.
- [ ] Behavior still reacts through `PatternResult`.
- [ ] Legacy AMP is not needed for main behavior.
- [ ] `PatternAssembler` and `PatternRules` are the only pattern-stage owners.
- [ ] Moving the types mostly requires imports/adapters, not re-deciding pipeline behavior.

---

## Recommended next pass name

```text
Pass E2 — Pattern Payload Hard Split
```

Goal:

```text
Move PatternCandidate and PatternResult out of DetectionPipeline.h
into the pattern module, while preserving current behavior.
```
