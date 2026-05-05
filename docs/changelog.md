# Changelog

## Unreleased

### Added
- New docs workflow files for `current-pass.md` and `changelog.md`.
- A reusable current-pass template for short, explicit agent task briefs.

### Changed
- The repository now treats `docs/myspec.md` as the stable architecture source of truth and keeps it out of per-pass task tracking.
- Analyzer `SEQ` logs now separate trial start, detection timing, and hit acceptance more clearly for humans.
- `docs/current-pass.md` now includes live `Done` and `In Progress` status sections.
- I2S sampling now carries explicit `AudioBlock` timing metadata, including block start sample index and approximate block start time.
- `AudioSignal` now processes I2S blocks directly while preserving the existing detector behavior.
- SEQ handling now keeps duplicate candidates as count-only events instead of letting them create or finalize trial results.
- SEQ runs now accept a manual `test=` label, with `default` as the fallback when no label is provided.
- SEQ now waits briefly before trial 1 so the first trigger does not race detector settle time.
- Loop-stress testing hooks were added for timing validation without changing detector thresholds.
- Current semi-stable tuning values are now documented in code and tracked here: onset=36, release=26, cooldown=300 ms, release debounce=30 ms, transient duration=60..240 ms, min strength=40, with baseline quiet thresholds of 40 for analog and 20 for I2S.
- SEQ currently uses a 500 ms warm-up before trial 1, and loop-stress validation defaults to `TEST_LOOP_DELAY_MS=20` unless overridden.
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
- `AudioSourceI2S` now tracks per-run read health counters and analyzer summaries print an `AUDIO summary` section.
- `AudioSourceI2S` now emits reusable `AudioBlock` metadata for each non-empty I2S refill and `AudioSignal` can process blocks directly.
- Diagnosis: event packaging is in the wrong layer.
- Decision: keep it for now, consume candidates in Analyzer, and move packaging out in a later dedicated pass.

### Fixed
- AudioSource stats now reset at session start, after rebase, so summaries reflect the active run instead of setup reads.
- `AUDIO stats reset` is printed once per analyzer session and not inside the audio-critical loop.
- I2S block timing metadata now flows through reusable source-owned storage instead of temporary stack pointers.
- Trial 1 no longer misses as often because SEQ now has a short warm-up delay before the first trigger.
- Duplicate candidates no longer alter trial outcome selection.
- Sampling and SEQ behavior are more stable under load because timing-critical work is less likely to be disrupted by logging and loop delay.

### Known Issues
- `AudioSignal` still packages events, which is the wrong abstraction.
- There is still some architecture drift between the intended pipeline and the current runtime split.
- Resonant detection is usable, but still underperforming a bit.
- Detection is still somewhat noise dependent.
- The physical setup still has high directionality, so placement matters more than it should.
- `AudioSignal::update(...)` is leftover migration code and should go away in a later pass.
- TODO/spec note: `AudioSignal` currently owns the AMP detector in the I2S path; `_audioOnsetDetector` is not active there.

### Notes
- `docs/myspec.md` was intentionally left unchanged in this pass.
- `docs/current-pass.md` was trimmed to a short task brief for the analyzer diagnostics pass.
- The `SEQ` output format was tightened without changing detector logic.
- The current pass has been closed out.
- Pass 3 adds block metadata without changing detector thresholds or RTOS/task architecture.

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
