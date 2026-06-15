# LOG-001 Process Changelog

Status: active

## Process Changelog

This changelog tracks the evolution of the LOG-001 logging workflow, especially when the runner, scaffold, or docs change together.

## What changed

- Added the scaffold helper (`tools/logging/+ create_log001_batch_scaffold.ps1`) for LOG-001 batch scaffolding.
- Added the campaign runner (`tools/logging/+ run_log001_campaign.ps1`) that writes `session.log`, `heartbeat.md`, `run_01.log` through `run_10.log`, `block_01_summary.md`, and `progress.md`.
- Standardized the launch shape to:

```text
SEQ start profile=TonalPulseScalar tries=50 mode=source when=all verbose=1
```

- Captured `PARAM STATUS` as the canonical full tuning snapshot.
- Kept `PARAM` echoing only changed fields.
- Recorded the applied tune from `PARAM STATUS` in the block summaries, so the logs do not confuse intended shifts with confirmed changes.

## Tuning ladder

The full 100-launch campaign is no longer baseline-only.

- Block 1: baseline snapshot.
- Block 2: shorten `scalar_max_duration_ms`.
- Block 3: lower `scalar_release_debounce_ms`.
- Block 4: shorten `scalar_max_duration_ms` again.
- Block 5: begin lowering `scalar_onset_threshold` and `scalar_release_threshold`.
- Block 6: reduce `scalar_release_debounce_ms` again.
- Block 7: continue threshold tightening.
- Block 8: continue duration tightening.
- Block 9: reduce debounce and thresholds again if still clean.
- Block 10: final tightening pass if the earlier blocks remain stable.

The block summaries should show the applied tune captured from `PARAM STATUS`, not only the requested `PARAM` command.

## Known runner notes

- The batch runner now records the current block tune in `progress.md`.
- The batch runner now records `campaign_state.json` and `heartbeat.md` so stopped runs can be diagnosed after the fact.
- The batch runner still writes the per-run transcript into `run_XX.log`.
- The batch folder is self-contained and can be replayed later.
- Tuning changes only take effect after the next `SEQ start`; an in-flight sequence keeps the parameters it started with.

## Current target

- 100 sequence launches total.
- 50 trials per launch.
- `mode source`.
- `when all`.
- `heartbeat.md` should be the first stop check if the terminal stops updating.

## Next check

- Watch for misses, duplicates, late results, `avg_dt_ms`, and `avg_strength`.
- Tighten only if the preceding block stays stable.
