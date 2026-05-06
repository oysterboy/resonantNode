# Changelog

## Changelog Rules

- Record every commit in a dated and named section.
- Keep `Known Issues` as a live rollover list.
- When a new commit lands, recheck each known issue and move it to `Fixed` if it is resolved.
- Leave only unresolved items in `Known Issues`.
- Keep this file historical and factual; use `docs/current-pass.md` for active work.

## 2026-05-06 - Pass A: Analyzer reference / parity check

### Added
- A stable current-pass template for short, explicit agent task briefs.

### Changed
- Pass A validated Analyzer as the trusted reference path for the current AMP/transient detection flow.
- Pass A confirmed the frozen detector baseline and the direct candidate-draining sequence in Analyzer.

### Fixed
- Analyzer reference questions are now answered against the current code instead of assumption.

### Notes
- Pass A did not introduce `DetectionPipeline`, `PatternCandidate`, or `PatternResult`.
- Pass B can begin from the Analyzer reference path.

## 2026-05-06 - Refactor baseline history

### Changed
- `docs/myspec.md` is the stable architecture source of truth for ResonantNode.
- `docs/current-pass.md` is the active pass tracker for refactor work.
- Analyzer `SEQ` logs separate trial start, detection timing, and hit acceptance more clearly.
- `AudioSignal` processes I2S blocks directly while preserving the existing detector behavior.
- SEQ handling keeps duplicate candidates as count-only events instead of letting them create or finalize trial results.
- SEQ runs accept a manual `test=` label, with `default` as the fallback when no label is provided.
- SEQ waits briefly before trial 1 so the first trigger does not race detector settle time.
- Loop-stress hooks exist for timing validation without changing detector thresholds.
- The current semi-stable tuning values are documented in code and tracked here: onset=36, release=26, cooldown=300 ms, release debounce=30 ms, transient duration=60..240 ms, min strength=40, with baseline quiet thresholds of 40 for analog and 20 for I2S.
- SEQ currently uses a 500 ms warm-up before trial 1, and loop-stress validation defaults to `TEST_LOOP_DELAY_MS=20` unless overridden.
- Analyzer `SEQ` duplicate detections are labeled explicitly.
- Analyzer `SEQ` onset logging prints each detector onset event directly so the analyzer view matches the detector more closely.
- Analyzer `SEQ` miss logs include the detector's last rejection reason when a transient is not accepted.
- Analyzer `SEQ` miss logs include onset-side rejection reasons as well.
- Analyzer no longer exposes the frequency detector as an alternate mode; analyzer runtime paths are AMP-only.
- Analyzer amplitude detector setup is folded directly into the source-specific I2S and analog configuration helpers.
- Analyzer `TUNE` has been removed; analyzer now focuses on `BASE`, `CAP`, and `SEQ`.
- Analyzer `SEQ` keeps onset rejection details in compact per-trial summaries instead of live per-loop spam.
- Analyzer `SEQ` no longer emits intermittent `SEQ status` lines during long runs.
- Analyzer `SEQ` maps all-below-threshold no-onset misses to a concise `quiet` summary instead of a large reject counter.
- Analyzer `SEQ` resets its primary hit buckets at the start of each run and reports primary completion separately in the final summary.
- Audio sampling now flows through explicit source availability and sample reads, and analyzer and resonant modes drain buffered samples with a per-loop cap.
- Analyzer `SEQ` reports empty-source loop counts in the final summary for buffered-sampling diagnostics.
- I2S audio sampling now buffers chunked input, reconstructs per-sample timestamps, and reports drop/backlog stats.
- Analyzer no longer exposes the `DET` command in analyzer mode.
- Analyzer AMP detector presets are frozen at onset=36, release=26, cooldown=300, minMs=60, maxMs=240, minStrength=40.
- Analyzer `SEQ` classifies accepted hits as early, expected, or late and reports those buckets in the final summary.
- Resonant behavior now uses the same frozen audio-signal and onset-detector parameters as analyzer.
- `AudioSourceI2S` now tracks per-run read health counters and analyzer summaries print an `AUDIO summary` section.
- `AudioSourceI2S` now emits reusable `AudioBlock` metadata for each non-empty I2S refill and `AudioSignal` can process blocks directly.

### Fixed
- AudioSource stats reset at session start and after rebase so summaries reflect the active run instead of setup reads.
- `AUDIO stats reset` is printed once per analyzer session and not inside the audio-critical loop.
- I2S block timing metadata now flows through reusable source-owned storage instead of temporary stack pointers.
- Trial 1 no longer misses as often because SEQ now has a short warm-up delay before the first trigger.
- Duplicate candidates no longer alter trial outcome selection.
- Sampling and SEQ behavior are more stable under load because timing-critical work is less likely to be disrupted by logging and loop delay.

### Known Issues
- KI-001: `AudioSignal` still packages events, which is the wrong abstraction.
- KI-002: There is still some architecture drift between the intended pipeline and the current runtime split.
- KI-003: Resonant detection is usable, but still underperforming a bit.
- KI-004: Detection is still somewhat noise dependent.
- KI-005: The physical setup still has high directionality, so placement matters more than it should.
- KI-006: `AudioSignal::update(...)` is leftover migration code and should go away in a later pass.
- KI-007: `AudioSignal` currently owns the AMP detector in the I2S path; `_audioOnsetDetector` is not active there.

### Notes
- `docs/myspec.md` is intentionally left unchanged by pass work unless the architecture itself changes.
- `docs/current-pass.md` should remain the short live task brief.
- Keep this file factual and historical; put active task instructions in `docs/current-pass.md`.

## 2026-04-30 - Analyzer frequency scaffold

### Added
- Switchable analyzer detector modes: `DET AMP` and `DET FREQ`.
- A frequency detector scaffold beside the existing amplitude detector.
- Frequency-score diagnostics for `BASE` and `CAP`.

### Changed
- Analyzer loop timing favored frequency mode so the detector could consume samples more aggressively.
- Sequence logging included clearer timing output for trial start, detector timing, and hit acceptance.

### Notes
- The amplitude detector stayed intact and remained the default path.
- The frequency detector was still a scaffold, but it was compiled and wired into the analyzer loop.
- The analyzer loop dropped the extra 1 ms delay in frequency mode so the detector could see more samples.

### Verification
- `platformio run -e esp32dev`

### Commit
- `3bb0694`
