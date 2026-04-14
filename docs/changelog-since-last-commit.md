# Changelog Since `33eaeea`

This branch refines the sound-reactive path from raw ADC input to transient-based behavior.

## Summary

- Replaced the older `LevelInput` path with a dedicated `AudioSignal` + `AudioOnsetDetector` pipeline.
- Shifted behavior decisions to transient-driven detection instead of onset-driven detection.
- Tuned the detector to reject ambient noise and stabilize peak closure timing.
- Reduced serial/debug noise so testing focuses on accepted transient events.

## Notable Changes

- Added `src/io/AudioSignal.*` for baseline tracking, smoothing, and magnitude extraction.
- Added `src/io/AudioOnsetDetector.*` for peak detection, hysteresis, release debounce, duration checks, and peak-strength filtering.
- Updated `src/behavior/ResonantBehavior.*` to react to transient events only.
- Updated `src/node/node.cpp` and `src/node/node.h` to wire the new detection flow and expose tuning parameters.
- Disabled chirp output and LED activity during detector calibration so self-feedback does not confuse the sweep.

## Calibration Notes

- The detector now reports accepted transients with timestamp, duration, and strength.
- Minimum transient peak strength was raised to suppress ambient noise crossings.
- The transient release threshold was made explicit so burst closure can be tuned directly.
- The current configuration is tuned for cleaner acceptance during the 10 ms to 200 ms sweep, but the measured acoustic duration is still a noisy proxy for the original drive length.

## Documentation

- Updated `docs/myspec.md` to reflect the current IO / detection / behavior split.

