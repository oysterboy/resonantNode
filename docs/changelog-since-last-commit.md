# Changed Since `69588d3`

This file is a per-commit changelog.

For each new commit, replace the commit hash in the heading and keep exactly one complete section for that commit.

## Commit `69588d3`

### Summary

- Refined `AudioOnsetDetector` into explicit onset and transient stages without changing its public API.
- Moved chirp pattern selection and self-chirp suppression timing into `ResonantBehavior`.
- Moved LED output timing, transient pulse handling, I2S telemetry, and chirp event logging into `NodeDebug`.
- Trimmed `Node` toward glue-only orchestration so it forwards state without owning output policy.
- Rewrote the architecture specs to use plain ASCII arrows and clearer `Node` ownership wording.
- Added a standalone `AnalyzerApp` path that runs `AudioSource -> AudioSignal -> AudioOnsetDetector` without `Node`, `Behavior`, or `ChirpOutput`.
- Flattened the top-level app wrapper so `main.cpp` now directly selects and owns either `Node` or `AnalyzerApp`.
- Moved runtime modes into explicit `src/modes/resonant/` and `src/modes/analyzer/` folders so mode entry points are siblings.
- Added an `EmitterApp` mode that listens on `Serial2` and drives `ChirpOutput` with requested frequency and duration.
- Simplified the current chirp output to a single-beep placeholder so compare mode can stay volatile until a richer chirp profile is needed.

### Notable Changes

- `src/io/AudioOnsetDetector.h`
- `src/io/AudioOnsetDetector.cpp`
- `src/behavior/ResonantBehavior.h`
- `src/behavior/ResonantBehavior.cpp`
- `src/node/node.h`
- `src/node/node.cpp`
- `src/node/node_debug.h`
- `src/node/node_debug.cpp`
- `src/modes/resonant/node.h`
- `src/modes/resonant/node.cpp`
- `src/modes/resonant/node_debug.h`
- `src/modes/resonant/node_debug.cpp`
- `src/modes/analyzer/AnalyzerApp.h`
- `src/modes/analyzer/AnalyzerApp.cpp`
- `src/modes/emitter/EmitterApp.h`
- `src/modes/emitter/EmitterApp.cpp`
- `src/main.cpp`
- `docs/myspec.md`
- `docs/refactor-spec.md`

### Verification

- `platformio run`

### Notes

- This section is complete for commit `69588d3`.
- Future commits should add their own new `## Commit <hash>` section and remove or replace older sections as needed.

### Next Commit Rule

- Use the next commit hash in the file heading.
- Keep one complete changelog section per commit.
- Include the same headings each time: `Summary`, `Notable Changes`, `Verification`, and `Notes`.
