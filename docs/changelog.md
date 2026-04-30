# Changelog

## Unreleased

### Added
- New docs workflow files for `current-pass.md` and `changelog.md`.
- A reusable current-pass template for short, explicit agent task briefs.

### Changed
- The repository now treats `docs/myspec.md` as the stable architecture source of truth and keeps it out of per-pass task tracking.
- Analyzer `SEQ` logs now separate trial start, detection timing, and hit acceptance more clearly for humans.
- `docs/current-pass.md` now includes live `Done` and `In Progress` status sections.
- The analyzer sequence-output pass is complete and `docs/current-pass.md` was cleared down to a finished state.
- Analyzer `SEQ` duplicate detections are now labeled explicitly.
- Analyzer `SEQ` onset logging now prints each detector onset event directly so the analyzer view matches the detector more closely.
- Analyzer `SEQ` miss logs now include the detector's last rejection reason when a transient is not accepted.
- Analyzer `SEQ` miss logs now include onset-side rejection reasons as well.
- Analyzer no longer exposes the frequency detector as an alternate mode; analyzer runtime paths are now AMP-only.
- Analyzer amplitude detector setup is now folded directly into the source-specific I2S and analog configuration helpers.
- Analyzer `TUNE` has been removed; analyzer now focuses on `BASE`, `CAP`, and `SEQ`.
- Analyzer `SEQ` now keeps onset rejection details in compact per-trial summaries instead of live per-loop spam.
- Analyzer `SEQ` no longer emits intermittent `SEQ status` lines during long runs.
- Analyzer `SEQ` maps all-below-threshold no-onset misses to a concise `quiet` summary instead of a huge reject counter.
- Analyzer `SEQ` now resets its primary hit buckets at the start of each run and reports primary completion separately in the final summary.
- Audio sampling now flows through explicit source availability and sample reads, and analyzer/resonant drain buffered samples with a per-loop cap.
- Analyzer `SEQ` now reports empty-source loop counts in the final summary for buffered-sampling diagnostics.
- I2S audio sampling now buffers chunked input, reconstructs per-sample timestamps, and reports drop/backlog stats.
- Analyzer no longer exposes the `DET` command in analyzer mode.
- Analyzer AMP detector presets are now frozen at onset=36, release=26, cooldown=300, minMs=60, maxMs=240, minStrength=40.
- Analyzer `SEQ` now classifies accepted hits as early, expected, or late and reports those buckets in the final summary.
- Resonant behavior now uses the same frozen audio-signal and onset-detector parameters as analyzer.

### Notes
- `docs/myspec.md` was intentionally left unchanged in this pass.
- `docs/current-pass.md` was trimmed to a short task brief for the analyzer diagnostics pass.
- The `SEQ` output format was tightened without changing detector logic.
- The current pass has been closed out.

---

## 2026-04-30 — Analyzer frequency scaffold

### Added
- Switchable analyzer detector modes: `DET AMP` and `DET FREQ`.
- A frequency detector scaffold beside the existing amplitude detector.
- Frequency-score diagnostics for `BASE` and `CAP`.

### Changed
- Analyzer loop timing now favors frequency mode so the detector can consume samples more aggressively.
- Sequence logging now includes clearer timing output for trial start, detector timing, and hit acceptance.

### Notes
- The amplitude detector stays intact and remains the default path.
- The frequency detector is still a scaffold, but it is compiled and wired into the analyzer loop.
- The analyzer loop drops the extra 1 ms delay in frequency mode so the detector can see more samples.

### Verification
- `platformio run -e esp32dev`

### Commit
- `3bb0694`
