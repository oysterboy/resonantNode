# TonalPulseScalar 3-Bin Logging Campaign

## Goal

Run the next LOG-001-style scalar logging campaign with `TonalPulseScalar` using the new 3-bin frequency-score stream.

The campaign should use the existing selector path:

- `profile.scalarTransient.observedStream = FeatureStreamId::FrequencyScore`

## Scope

This pass is for the next logging campaign only.

In scope:

- one campaign folder for the whole run
- 10 blocks of 1 launch
- `TonalPulseScalar` as the active profile
- the existing 3-bin `FrequencyScore` scalar stream
- the standard LOG-001 summary files and per-run logs
- recording the requested tune and the applied tune per block

Out of scope:

- new detector logic
- new analyzer fields
- new `ScalarInputMode` plumbing
- Hann windowing
- broad refactors outside the logging workflow

## Allowed Files

- `tools/logging/+ create_log001_batch_scaffold.ps1`
- `tools/logging/+ run_log001_campaign.ps1`
- `tools/logging/tuning-run-process.md`
- `tools/logging/process-changelog.md`
- generated files under `tools/logs/seq-tests/<campaign-folder>/`

## Forbidden Files

- `docs/myspec.md`
- unrelated detection source files
- unrelated behavior/output code
- any broad refactor outside the campaign runner and its generated logs

## Exact Changes

1. Create one new campaign folder for this pass.
2. Use 10 blocks of 1 launch.
3. Keep the run shape aligned with the current LOG-001 process:
   - `SEQ start profile=TonalPulseScalar tries=50 mode=source when=all verbose=1`
4. Keep the scalar profile on the existing 3-bin frequency-score path.
5. Use the current two-phase tuning ladder:
   - blocks 1-5: hold `scalar_max_duration_ms=220` and `scalar_onset_threshold=19000`, then sweep `scalar_release_debounce_ms` from `30` down to `10`
   - blocks 6-10: keep the best debounce from phase 1, then sweep `scalar_release_threshold` from `5000` down to `1000`
6. Record the requested tune and the confirmed applied tune in each block summary.

## Success Criteria

- The campaign is organized as one folder with 10 blocks of 1.
- The active profile is `TonalPulseScalar`.
- The scalar input uses `FeatureStreamId::FrequencyScore`.
- Each block summary reflects the applied tune, not just the requested tune.
- The run artifacts include `README.md`, `session.log`, `heartbeat.md`, `campaign_state.json`, `progress.md`, `run_01.log` through `run_10.log`, and `block_01_summary.md` through `block_10_summary.md`.

## Verification

- confirm the scaffold/runner created a single campaign folder
- confirm the block summaries show the requested and applied tuning values
- confirm the per-run logs are present for all 10 blocks
- sanity-check that the campaign stayed on `TonalPulseScalar` with `FrequencyScore`
