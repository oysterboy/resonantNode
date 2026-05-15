# Codex Pass - Section K: Documentation / Spec Alignment

Version: Detection Roadmap v0.3 - Pass K
Scope: Align the docs with the implemented detection and profile boundaries.

---

## Goal

Document the current detection vocabulary and profile-proof boundary.

Current code now establishes:

```text
FeatureExtractor / FeatureStream / FeatureHistory
SignalEmitter / SignalDetector / SignalInspector
PatternAssembler / PatternRules
FieldState
DetectionProfile
```

Current boundary:

```text
SignalCandidate -> InspectedSignal -> PatternCandidate -> PatternResult
Behavior consumes PatternResult + FieldState
FrequencyMatchDetector owns frequency lifecycle only
SignalInspector can add AMP locality and history-backed evidence
PatternAssembler owns signal grouping
PatternRules own pattern meaning
DetectionProfile owns profile composition
```

Current proof profiles:

```text
FreqAmpProfile
AmpStateProfile
ChirpProfile
```

Do not add external JSON/YAML profile config.
Do not add a plugin registry.
Do not add new detection behavior in this pass.

---

## K Checklist

- [x] Update the detection roadmap overview to reflect the current signal-vs-pattern pipeline and profile-proof scope.
- [x] Update the architecture spec detection section with the implemented names and boundaries.
- [x] Document the stable naming set.
- [x] Document the signal-vs-pattern split.
- [x] Document the `FrequencyMatchDetector` boundary.
- [x] Document AMP locality inspection.
- [x] Document `FeatureHistory` / `ScalarWindow` usage.
- [x] Document the `FieldState` boundary.
- [x] Document the `PatternAssembler` role.
- [x] Document the `PatternRules` role.
- [x] Document the behavior input boundary.
- [x] Document current proof profiles in more detail.

The current proof profiles are:

- `FreqAmpProfile`: frequency-first baseline with AMP locality inspection.
- `AmpStateProfile`: AMP/transient profile that proves the field-state boundary.
- `ChirpProfile`: the first multi-signal proof profile, selectable in code.

## Test Notes

The docs match the current code if:

- `DetectionProfile` is the top-level code-defined profile object.
- `FreqAmp`, `AmpState`, and `Chirp` are selectable.
- `Behavior` still consumes `PatternResult + FieldState`.
- `FrequencyMatchDetector` still owns only frequency candidate lifecycle.
