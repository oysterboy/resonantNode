# LOG-001 Seq Tests Guide

Status: active

## Goal

- Keep the scalar tuning workflow log-first and reproducible.
- Use the canonical launch shape:

```text
SEQ start profile=TonalPulseScalar tries=50 mode=source when=all verbose=1
```

- Tune from saved batch folders instead of pasted serial snippets.

## Current focus

- Profile: `TonalPulseScalar`
- Default tuning target: `Scalar`
- Active command surface: `PARAM` and `PARAM STATUS`
- Run planning: 100 launches in 10 launch blocks
- Block size: 10 launches

## Logging layout

Each batch folder should contain:

- `README.md`
- `session.log`
- `heartbeat.md`
- `campaign.lock`
- `campaign_state.json`
- `run_01.log` through `run_10.log`
- `block_01_summary.md` through `block_10_summary.md`
- `progress.md`

The batch README should capture:

- the parameter snapshot
- the launch command
- the port and baud rate
- the decision rule for the next block
- the failure and resume markers if the batch stops early

## What to capture

Capture and compare:

- miss count
- duplicate count
- late count
- `avg_dt_ms`
- `avg_strength`

Keep `PARAM STATUS` snapshots:

- before each block
- after each parameter change
- after each block decision if the tuning changes

## Workflow modes

- `Codex-run`: Codex or the helper launches the block, reads the summary, and applies the next `PARAM` shift.
- `User-run`: the helper prints the exact commands and log targets for manual execution.
- Interrupted batches can be resumed from the saved folder with `-BatchRoot` and `-StartRun`.
- If a run stops unexpectedly, check `session.log`, `progress.md`, `campaign_state.json`, and `heartbeat.md` together.

## Tuning rule of thumb

- First remove misses.
- Then tighten `maxTransientDurationMs` and hysteresis in small steps.
- Keep duplicates at zero if possible.
- Avoid changing more than one knob at a time unless the block is clearly unstable.

## Helper

Use the checked-in helper under `tools/` to create the batch scaffold and write the README and summary placeholders in the expected layout.

## Notes

- Do not depend on ad-hoc pasted serial text for tuning comparisons.
- Keep each batch folder self-contained.
- If a block regresses, keep the last good values and shrink the next adjustment.
