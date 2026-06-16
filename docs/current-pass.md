# TonalPulseScalar Two-Sweep Logging Campaign

## Goal

Run the next LOG-001 scalar campaign as a 10-block, 1-launch-per-block pass for `TonalPulseScalar`.

Keep the current scalar defaults in place except for the two sweeps below.

## Scope

This pass is only for the next logging campaign.

In scope:

- one campaign folder for the whole run
- 10 blocks of 1 launch
- `TonalPulseScalar` as the active profile
- the current 3-bin frequency stream wiring
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
4. Keep all non-swept scalar defaults fixed.
5. Sweep 1, release threshold:
   - hold the other scalar settings steady
   - move `scalar_release_threshold` upward from `5000` to `15000`
   - use 5 blocks with `2500`-point steps: `5000`, `7500`, `10000`, `12500`, `15000`
6. Sweep 2, attack threshold:
   - keep the release value chosen from sweep 1 fixed
   - increase `scalar_onset_threshold` in `1000`-point steps
   - keep the remaining scalar settings fixed
   - use 5 blocks starting from the current onset default and stepping up by `1000` each block: `19000`, `20000`, `21000`, `22000`, `23000`
7. Record the requested tune and the confirmed applied tune in each block summary.

## Success Criteria

- The campaign is organized as one folder with 10 blocks of 1.
- The active profile is `TonalPulseScalar`.
- The campaign performs the release-threshold sweep first and the attack-threshold sweep second.
- Each block summary reflects the applied tune, not just the requested tune.
- The run artifacts include `README.md`, `session.log`, `heartbeat.md`, `campaign_state.json`, `progress.md`, `run_01.log` through `run_10.log`, and `block_01_summary.md` through `block_10_summary.md`.

## Verification

- confirm the scaffold/runner created a single campaign folder
- confirm the block summaries show the requested and applied tuning values
- confirm the per-run logs are present for all 10 blocks
- sanity-check that the campaign stayed on `TonalPulseScalar`
