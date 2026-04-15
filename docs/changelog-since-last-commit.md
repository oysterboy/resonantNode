# Changelog Since `3e64a2b`

This branch extends the audio input path with a source abstraction and an I2S-ready implementation path.

## Summary

- Added a stable `AudioSource` abstraction so the rest of the pipeline can stay sample-based.
- Added `AudioSourceAnalog` and `AudioSourceI2S` as interchangeable source implementations.
- Kept `AudioSignal`, `AudioOnsetDetector`, and behavior logic unchanged while the input seam was refactored.
- Wired `Node` to select a source implementation at construction time.

## Notable Changes

- Added `src/hal/AudioSource.h` as the shared acquisition contract.
- Added `src/hal/AudioSourceAnalog.*` to wrap the existing ADC-backed path.
- Added `src/hal/AudioSourceI2S.*` to prepare the digital mic path behind the same public API.
- Updated `src/io/AudioSignal.*` to depend on `AudioSource` instead of the concrete ADC wrapper.
- Updated `src/node/node.cpp` and `src/node/node.h` to choose the source implementation and begin acquisition before signal shaping.
- Kept `AudioOnsetDetector` naming and behavior intact so the refactor stays mechanical.

## Calibration Notes

- The current branch still uses the analog source by default.
- The I2S source is wired in but not selected by default yet.
- The public source contract remains `begin()` plus `readSample()`, which keeps downstream code stable.

## Documentation

- Updated `docs/myspec.md` to reflect the current IO / detection / behavior split.
- Updated `docs/refactor-spec.md` to define the next-step `AudioSourceI2S` work.
