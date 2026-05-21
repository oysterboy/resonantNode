# LegacyRemoval

ResonantNode / Resonanzraum detection refactor tracker.

## Current Status

- Core legacy-path removals are in place in active source.
- Analyzer compatibility cleanup in active source is done.
- Active docs are aligned; historical refactor docs remain as history.
- Analyzer boundary cleanup is done in the normal path.
- `AmpTransientDetector` stays.

## Locked Decisions

- Node and Analyzer are I2S-only, DetectionRuntime-only, profile-configured.
- No `DetectionMode`.
- No `AmpState`.
- No `useLegacyPath`.
- No legacy candidate-builder folder in active source.
- Analyzer SEQ consumes `PatternResult` / `FieldState` from `DetectionRuntime` only.
- `RAW_SAMPLE_CAPTURE` stays separate.
- `AmpTransientDetector` stays as runtime support.

## Done

- Removed the AnalogMic path.
- Removed `DetectionMode`.
- Removed `AmpState`.
- Removed `useLegacyPath`.
- Removed the legacy candidate-builder path from active source.
- Removed the remaining analyzer compatibility shims and legacy summary labels from active source.
- Renamed Analyzer compatibility storage to neutral trial-report storage and removed dead parity bookkeeping.
- Removed the remaining Analyzer parity scaffolding from `AnalyzerReporting.h`.
- Reworded transitional comments and payload notes in the detection/pattern helpers.
- Removed the Analyzer live-frequency fallback path and its `liveFrequencyOnly` mode handling.
- Kept `AmpTransientDetector` instead of deleting it in Pass 10.

## Pending

- Keep historical refactor docs as history, but do not treat them as active implementation guidance.

## Working Rule

For each remaining pass:

1. Change only what belongs to that pass.
2. Verify the code still builds or reaches the expected runtime state.
3. Commit the pass on its own.

## Final Target Shape

### Node

- I2S only.
- `DetectionProfile` config only.
- `DetectionRuntime` only.
- Consumes `PatternResult` + `FieldState`.
- No `DetectionMode`.
- No legacy candidate builder.

### Analyzer

- I2S only.
- `DetectionRuntime` only.
- `SEQ_TRIAL`, `SEQ_EXPLAIN`, and `SEQ_SUMMARY` come from actual pipeline `PatternResult` + `FieldState`.
- Analyzer consumes actual pipeline results in the normal path.
- `RAW_SAMPLE_CAPTURE` is separate.
- No legacy SEQ/report aliases.

### Detection

- No legacy builder folder in active source.
- No `AmpState`.
- No `useLegacyPath`.
- AMP remains only as current runtime signal support, not as a legacy path.
