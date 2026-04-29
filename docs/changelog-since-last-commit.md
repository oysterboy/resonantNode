# Changed Since `2eccd81`

This file is a per-commit changelog.

For each new commit, replace the commit hash in the heading and keep exactly one complete section for that commit.

## Commit `2eccd81`

### Summary

- Added a second tone backend for BTL-style piezo drive using two GPIOs with one inverted.
- Kept the existing single-pin piezo backend as the default tone implementation.
- Let `Node` and `EmitterApp` choose between single-pin and BTL tone output by constructor configuration.
- Kept `ChirpOutput` as the timing/policy layer and left sample-style DAC/I2S output for later.
- Consolidated `myspec.md` and `refactor-spec.md` into one current-architecture file with a dedicated next-steps section.
- Added a `BASE` analyzer command that measures quiet baseline and noise-floor statistics before chirp tuning.

### Notable Changes

- `src/hal/PiezoToneOutputBTL.h`
- `src/hal/PiezoToneOutputBTL.cpp`
- `src/modes/resonant/node.h`
- `src/modes/resonant/node.cpp`
- `src/modes/emitter/EmitterApp.h`
- `src/modes/emitter/EmitterApp.cpp`
- `docs/myspec.md`
- `docs/refactor-spec.md`

### Verification

- `platformio run`

### Notes

- This section describes the current uncommitted work since `2eccd81`.
- Sample-style output backends for DAC and I2S are intentionally deferred.
- Analyzer sequence tuning now uses the requested min-strength range instead of a tiny hardcoded window, and the debounce/duration sweep ranges are widened around the current detector settings.
- `TUNE` now accepts explicit stage ranges for `minStrength`, `releaseDebounce`, and `minDuration` in one command.
- Use `BASE` first to capture the quiet floor, note `quiet_raw_peak` and `quiet_delta_max`, pick detector thresholds above that peak, then run `CAP`, `TUNE`, and `SEQ` to refine and verify the final settings.

### Next Commit Rule

- After the next commit, replace the hash in the file heading with that new commit hash.
- Keep one complete changelog section per commit.
- Include the same headings each time: `Summary`, `Notable Changes`, `Verification`, and `Notes`.
