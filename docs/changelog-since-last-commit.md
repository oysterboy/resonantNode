# Changed Since `005ddff`

This file is a per-commit changelog.

For each new commit, replace the commit hash in the heading and keep exactly one complete section for that commit.

## Commit `005ddff`

### Summary

- Added a switchable frequency detector scaffold beside the existing amplitude detector.
- Let the analyzer toggle between amplitude and frequency detection with `DET AMP` and `DET FREQ`.
- Added a faster analyzer loop path for frequency mode so the new detector can consume samples more aggressively.
- Added frequency-score diagnostics so `BASE` and `CAP` can show target power, neighboring-bin power, and spectral contrast.

### Notable Changes

- `src/io/AudioFrequencyDetector.h`
- `src/io/AudioFrequencyDetector.cpp`
- `src/modes/analyzer/AnalyzerApp.h`
- `src/modes/analyzer/AnalyzerApp.cpp`
- `src/main.cpp`

### Verification

- `platformio run -e esp32dev`

### Notes

- This section describes the current uncommitted work since `005ddff`.
- The amplitude detector stays intact and remains the default path.
- `DET AMP` and `DET FREQ` are the current selector commands.
- The frequency detector is still a scaffold, but it now compiles and is wired into the analyzer loop.
- The analyzer loop now drops the extra 1 ms delay in frequency mode so the detector can see more samples.

### Next Commit Rule

- After the next commit, replace the hash in the file heading with that new commit hash.
- Keep one complete changelog section per commit.
- Include the same headings each time: `Summary`, `Notable Changes`, `Verification`, and `Notes`.
