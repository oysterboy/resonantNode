# Changelog

## Changelog Rules

- Record every commit in a dated and named section (including commit id)
- Keep `Changes Since Last Commit` as the staging area for notes that will move into the next commit section.
- When committing, move any `Changes Since Last Commit` note into the dated and named commit section.
- Keep `Known Issues` as a live rollover list.
- When a new commit lands, recheck each known issue and move it to `Fixed` if it is resolved.
- Leave only unresolved items in `Known Issues`.
- Keep this file historical and factual; use `docs/current-pass.md` for active work.

## Changes Since Last Commit

- Clarified Pass 2 docs/comments so the amplitude path and frequency path are both framed as stream-extractor plus scalar-transient-detector concepts.
- Renamed `AudioOnsetDetector` to `AmpTransientDetector`, updated its include paths, and kept the AMP-side facade comment aligned with the new name.
- Added `FreqTransientDetector` as the public FREQ-side facade and kept `FrequencyBandStreamExtractor` as its live evidence engine.
- Kept runtime behavior unchanged while refining the current-pass wording around the AMP/FREQ ownership pair.
- Added side-by-side early/full frequency evidence logging in the resonant and analyzer candidate summaries to make tonal validity failures easier to inspect without changing detector behavior.
- Kept the current AMP/transient A-path and `requireTonal` gating unchanged while improving the visibility of candidate-window retrospective checks.
- Normalized `docs/current-pass.md` Phase 0 so it uses `myspec.md` as the authority for roadmap wording and current architecture vocabulary.
- Updated `src/io/AudioSignal.h` comments to describe the current signal / candidate assembly role instead of a detector wrapper.
- Updated `src/detection/DetectionPipeline.h` comments to call out detector candidates as transitional payloads inside the pattern pipeline.
- Added shared `FrequencyCandidate` and `FrequencyCandidateBuilder` types, moved live frequency candidate state out of Analyzer, and introduced a neutral `AudioSignalFrame` snapshot boundary.
- Extracted `AmpCandidateBuilder` so AMP candidate state no longer lives inside `AudioSignal`.
- Reduced `AudioSignal` to signal/history ownership and renamed its reset helper to `resetSignalState`.
- Kept `ScalarTransientDetector` as the shared transient core underneath `AmpTransientDetector`, while the live frequency path now uses `FreqTransientDetector` over `FrequencyBandStreamExtractor`.
- Promoted the cleaned live frequency candidate path into the normal Analyzer SEQ handoff so a valid `FrequencyCandidate` can now finalize a trial result when AMP has not already accepted it.

## 2026-05-13 - Detector ownership and RB timing cleanup

### Changed
- Normalized project defaults to `3200 Hz` across the build settings and runtime fallbacks while keeping the resonant first idle chirp at `2000 Hz`.
- Kept `ResonantBehavior` on `requireTonalForBehavior = true` and aligned the RB help/setup notes with that default.
- Moved RB idle timing to a `30000 ms` default and kept the startup idle chirp at `500 ms` / `2000 Hz` / `200 ms` gap.
- Kept RB boot logging silent by default and trimmed `RB log full` down to compact candidate summaries so logging stays out of the timing loop.
- Kept RB detect-only listen-only, with chirp suppression, idle suppression, and response suppression while still reporting detector candidates.
- Kept RB detect-only LED pulses at full brightness for tonal-valid hits and 50% brightness for transient-only hits.
- Added passive `SEQ OBS` for an already-running external emitter, with no emitter claim, no chirp output, and a full `2000 ms` observe window.
- Kept `SEQ stop` as the shared stop command for both normal `SEQ` and passive `SEQ OBS`.
- Split detector ownership so `AudioSignal` now uses an injected `AudioOnsetDetector` instead of owning the onset detector internally.
- Kept `AudioSignal` responsible for signal conditioning and candidate packaging, while `AudioOnsetDetector` remains the detector owner in the node and analyzer layers.
- Kept RB response timing at `wait=100 ms` and self-chirp suppression at `200 ms`, and exposed idle timing as `idleTimeout`, `idleTimeoutVariation`, `idleBlockedAfterHeard`, and `idleBlockedAfterOwnEmit`.
- Updated the detector baseline defaults to onset=23 and release=20 in both RB and Analyzer runtime setup/help text.
- Current baseline remains the 5-node work-in-progress setup.
- Idle rescheduling now follows the same accepted-response gate as an actual chirp response, instead of resetting on any valid candidate.

### Verification
- `platformio run -e esp32dev`

## 2026-05-11 - Rename chirp wording to transient wording

### Changed
- Renamed the tonal classifier target from `ValidTonalChirp` to `ValidTonalTransient` in the shared pipeline and classifier logic.
- Updated Analyzer runtime logs to print `valid_tonal_transient` for the current tonal click/beep path.
- Aligned the current-pass, spec, and setup notes wording to describe the current path as a tonal transient instead of a chirp.
- Kept runtime behavior unchanged.

### Verification
- `platformio run`

### Notes
- Commit id `d2a5358`.

## 2026-05-11 - Cleaned test candidate wording and baseline notes

### Changed
- Added explicit current-implementation wording to `docs/setupNotes.md`, `docs/current-pass.md`, and `docs/myspec.md` describing the AMP/transient event window and candidate-window frequency classification flow.
- Aligned the setup notes and analyzer help text to the intended `onset=30 release=20` baseline.
- Updated the setup notes to treat `freqEarly_score >= 50000` and `freqEarly_contrast >= 5` as the intended early-frequency rule.
- Kept runtime behavior unchanged.

### Verification
- `platformio run`

### Notes
- Commit id `db7db99`.

## 2026-05-11 - Readability cleanup: mark legacy shims and placeholders

### Changed
- Marked the old `ResonantBehavior` bool-based update path as a legacy shim.
- Marked the surviving single-sample `AudioSignal::update(...)` path as a legacy entry point from the block-processing migration.
- Marked the `AudioSignal` detector ownership bridge and `ChirpOutput` placeholder section as legacy so the remaining transitional code is explicit.
- Kept runtime behavior unchanged.

### Verification
- `platformio run`

### Notes
- Commit id `5e7f4ef`.

## 2026-05-11 - Clarify shared signal and detector boundaries (41f3509)

### Changed
- Added module-level comments to the shared signal, onset, and frequency detector headers so their ownership boundaries are explicit.
- Added section markers to `AudioSignal.cpp`, `AudioOnsetDetector.cpp`, and `AudioFrequencyDetector.cpp` to make the runtime flow easier to scan.
- Added a shared module comment to `AudioDebugConfig.h` to clarify that it supplies cross-mode debug and test defaults.
- Kept the detector and signal behavior unchanged.

### Verification
- `platformio run`

## 2026-05-11 - Analyzer structure cleanup first (e76f755)

### Changed
- Added a module-level comment block to `AnalyzerApp.h` and `AnalyzerApp.cpp` describing analyzer responsibilities and file structure.
- Reordered analyzer session state and helper declarations so the file reads more like the runtime flow.
- Added section markers for setup, runtime, sessions, raw trigger handling, and reporting.
- Kept analyzer behavior unchanged.

### Verification
- `platformio run`

## 2026-05-11 - Refine node and behavior file structure (d636d11)

### Changed
- Reordered `ResonantBehavior` members and helpers into lifecycle, configuration, output, and inspection sections.
- Added file-structure comments to `ResonantBehavior.cpp`, `node.h`, and `node.cpp`.
- Grouped node members and helpers by runtime responsibility so the Resonant path is easier to scan.
- Kept runtime behavior unchanged.

### Verification
- `platformio run`

## 2026-05-11 - Clarify self suppression timing (485b7ec)

### Changed
- Clarified the Resonant self-suppression state split between behavior-level gating and detection-side suppression timing.
- Renamed the detection-side suppression window to make its detector/pipeline scope explicit.
- Added comments to explain that the tail window only changes outcome when it outlasts the refractory window.
- Kept the underlying suppression behavior unchanged.

### Verification
- `platformio run`

## 2026-05-08 - Pass 6: Classifier integration refactor

### Changed
- The shared frequency helper was renamed to `FrequencyEvidenceEvaluation` to match its classifier role.
- The helper now owns the shared frequency evaluation, reject-reason mapping, and classifier staging logic.
- `PatternResult` now carries classifier-facing fields such as `candidateValid`, `tonalValid`, `behaviorEligible`, and `rejectReason`.
- Analyzer and RB logs now print the classifier split alongside the existing frequency diagnostics.
- Candidate and SEQ headline frequency logs now use the classified `patternResult.freq` snapshot instead of the raw detector-only fields, so `freq_matched`, `freq_conf`, and related values line up with the classifier result.
- Analyzer SEQ now reports classifier-level counters for tonal primaries, tonal/non-tonal duplicates, tonal/non-tonal unexpected hits, and frequency reject buckets.
- ResonantBehavior now accepts `requireTonal=0/1`, and RB logs `RB_BLOCK` when a non-tonal candidate is gated out.
- The SEQ frequency-class summary now reports `valid_tonal_chirp` versus `transient_only` and shows the frequency evaluation reason.
- RB behavior handling now keys off `candidateValid` so the new classification names can be logged without changing the current reaction behavior.
- The active pass note now records step 5 as complete, with the headline logging semantics aligned to the classifier snapshot.

### Verification
- `platformio run -e esp32dev`
- `platformio run -e esp32dev-analyzer`

### Notes
- Commit id `bfe10a7`.

## 2026-05-08 - Stabilize AMP detection (21f43bd)

### What that means in practice

- The project has moved from a sampling-only Resonant baseline to a shared detector pipeline with richer evidence tracking.
- Analyzer and Resonant now start from the same detector defaults, and both can be tuned at runtime instead of staying frozen.
- Frequency analysis is now visible in SEQ logs as diagnostic evidence, with early-window and full-window views side by side.
- The serial output is more readable now: misses, duplicates, reject reasons, and end-of-run summaries are all easier to scan.
- Resonant behavior tuning is separated from detector tuning, so the mode-specific knobs stay clearer.

### Changed
- Analyzer and Resonant now share the same detector baseline defaults at onset=30 and release=20.
- `PARAM` now sets detector tuning in Analyzer, and `RB PARAM` does the same in Resonant.
- `RB BEHAV` now carries the RB-only behavior timing knobs, separate from detector tuning.
- Frequency logging tuning is now configurable through `freqScore` and `freqContrast`, but still does not affect candidate acceptance.
- The frequency tuning helper now centralizes the shared threshold evaluation and fail-reason formatting for Analyzer and RB.
- Sequence verbose output now carries compact per-trial diagnostics for miss reasons, reject breakdowns, and frequency evidence.
- Verbose candidate logging now includes explicit frequency fail reasons such as `freqContrast too low` and `freq score too low`.

### Verification
- `platformio run -e esp32dev-analyzer`

### Notes
- Frequency detection results are logged for comparison only and are not used for acceptance yet.
- The tuned parameters are meant to restore relatively reliable detection after the I2S stream fixes.
- Commit id `21f43bd`.

## Commit History Since Resonant Sampling Baseline

## 2026-05-08 - FREQ logging safety backup (d3e6a24)

### Changed
- The per-trial verbose `SEQ` report now records `miss_reason`, `max_env`, `max_strength_est`, `onset_seen`, and reject counters.
- The verbose end-of-run dump now prints compact reject totals for `too_short`, `too_long`, `below_strength`, and `blocked`.
- Trial snapshots now preserve the strongest observed estimate across accepted, duplicate, and rejected paths so the end report can show a useful peak.

### Verification
- `platformio run -e esp32dev-analyzer`

### Notes
- This commit is log-only; behavior remains unchanged.

## 2026-05-08 - Rename sequence logging and add end reports (2addd1b)

### Changed
- Pass 1 stabilized `AudioSignal` ownership comments around the transient detector boundary without changing detector behavior.
- Pass 2 added a bounded centered-sample history to `AudioSignal` and exposed readback helpers for candidate-window analysis.
- Pass 3 added a candidate-window frequency probe over the raw history and threaded it into Analyzer and Resonant.
- Pass 4 made the early-window result explicit in logs as `freqEarly_*` alongside the live rolling frequency snapshot.
- The sequence log vocabulary now uses `report` for the end-of-seq verbose dump and `liveraw` for noisy live diagnostics, with older aliases still accepted for compatibility.
- `SEQ stop` now flushes the stored per-trial verbose report before shutdown, so the full comparison dump is available both at natural completion and manual stop.
- The compact `SEQ_TRIAL` line now includes `freqEarly`, `freqFull`, and `full_ratio` so the early-vs-full comparison stays visible during live runs.
- Analyzer and Resonant candidate logs now include `onset_sample`, `peak_sample`, `release_sample`, and `peak_ms` so timing diagnostics use one vocabulary.
- Analyzer `SEQ help` now lists the accepted `SEQ` inputs and the main output fields in serial form.

### Verification
- `platformio run -e esp32dev-analyzer`

### Notes
- This commit is log-only; behavior and `PatternResult.valid` are unchanged.

## 2026-05-06 - H2: Frequency Evidence Observer

### Changed
- `AudioFrequencyDetector` is now sampled alongside the existing transient flow in Analyzer and Resonant.
- `PatternResult` can carry attached frequency evidence snapshots without changing validity, type, or behavior.
- Analyzer and Resonant candidate logs now include the inert frequency fields where they already print pattern details.

### Notes
- H2 is observation-only and does not change the detector or behavior rules.
- Commit id pending until this snapshot is committed.

## 2026-05-06 - H1: Pattern Evidence Scaffold

### Changed
- `DetectionPipeline` now carries inert transient and frequency evidence fields alongside the existing flat candidate data.
- `PatternCandidate` mirrors accepted detector data into `candidate.transient`, while frequency evidence remains default-inactive.
- Analyzer and Resonant candidate logs now include the inert evidence fields where they already print pattern details.

### Notes
- H1 is architecture-only and does not change behavior.
- Commit id pending until this snapshot is committed.

## 2026-05-06 - Refactor: DetectorPipeline Baseline

### Changed
- Pass G is complete in code: autonomous Resonant behavior is back on the `PatternResult` path, detection-only remains a safe fallback, behavior decisions are explainable, and the serial interface now has a minimal logging mode.
- The shared pipeline still carries `processedAtMs`, `AudioSignal` exposes candidate queue depth, and Analyzer/Resonant logs show processing lag and queue depth alongside the existing onset/duration parity fields.
- The frozen AMP cooldown remains `25 ms` in both Analyzer and Resonant setup.

### Notes
- This snapshot closes Pass G and folds the finished state into the dated history.
- The next commit should carry this section forward with the commit id.

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
