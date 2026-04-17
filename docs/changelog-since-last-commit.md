# Changelog Since `3e64a2b`

This branch extends the audio input path with a source abstraction and an I2S-ready implementation path.

## Biggest Factor

- Added the MEMS mic path and tuned the detector around it.

## Summary

- Added a stable `AudioSource` abstraction so the rest of the pipeline can stay sample-based.
- Added `AudioSourceAnalog` and `AudioSourceI2S` as interchangeable source implementations.
- Kept `AudioSignal`, `AudioOnsetDetector`, and behavior logic unchanged while the input seam was refactored.
- Wired `Node` to select a source implementation at construction time.
- Extracted debug and plot-state tracking into `src/node/node_debug.*` so `Node` stays focused on orchestration.
- Updated the detector/logging path for the MEMS mic so accepted and rejected transients are visible.

## Notable Changes

- Added `src/hal/AudioSource.h` as the shared acquisition contract.
- Added `src/hal/AudioSourceAnalog.*` to wrap the existing ADC-backed path.
- Added `src/hal/AudioSourceI2S.*` to prepare the digital mic path behind the same public API.
- Updated `src/io/AudioSignal.*` to depend on `AudioSource` instead of the concrete ADC wrapper.
- Updated `src/node/node.cpp` and `src/node/node.h` to choose the source implementation and begin acquisition before signal shaping.
- Added `src/node/node_debug.*` to own debug latches, loop timing stats, and serial plot formatting.
- Kept `AudioOnsetDetector` naming and behavior intact so the refactor stays mechanical.
- Added self-chirp suppression around the chirp tail so the MEMS mic is less likely to retrigger on ring-down.
- Moved chirp emit-frequency setup into `Node::configureParameters()` so the node owns its output tuning.
- Switched the LED feedback to three transient pulses, full-bright emit, 70% self-ignore, and 50% refractory.
- Added throttled peak-start / peak-open debug logging to `AudioOnsetDetector` for transient troubleshooting.

## Calibration Notes

- The current branch still uses the analog source by default.
- The I2S source is wired in and selected for the current MEMS mic setup.
- The public source contract remains `begin()` plus `readSample()`, which keeps downstream code stable.

## Documentation

- Updated `docs/myspec.md` to reflect the current IO / detection / behavior split.
- Updated `docs/refactor-spec.md` to define the next-step `AudioSourceI2S` work.
