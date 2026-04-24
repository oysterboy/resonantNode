# Changed Since `8632015`

This file is a per-commit changelog.

For each new commit, replace the commit hash in the heading and keep exactly one complete section for that commit.

## Commit `8632015`

### Summary

- Split chirp output into timing/policy and a reusable tone-output HAL.
- Added a concrete piezo tone backend that drives the current GPIO piezo hardware.
- Kept `ChirpOutput` as the sequencing layer so higher-level code still starts and stops chirps the same way.
- Wired both the resonant node mode and the emitter mode through the shared tone backend.
- Left sample-style output backends for DAC and I2S as a future step.

### Notable Changes

- `src/hal/ToneOutput.h`
- `src/hal/PiezoToneOutput.h`
- `src/hal/PiezoToneOutput.cpp`
- `src/io/ChirpOutput.h`
- `src/io/ChirpOutput.cpp`
- `src/modes/resonant/node.h`
- `src/modes/resonant/node.cpp`
- `src/modes/emitter/EmitterApp.h`
- `src/modes/emitter/EmitterApp.cpp`

### Verification

- `platformio run`

### Notes

- This section describes the current uncommitted work since `8632015`.
- Sample-style output backends for DAC and I2S are intentionally deferred.

### Next Commit Rule

- After the next commit, replace the hash in the file heading with that new commit hash.
- Keep one complete changelog section per commit.
- Include the same headings each time: `Summary`, `Notable Changes`, `Verification`, and `Notes`.
