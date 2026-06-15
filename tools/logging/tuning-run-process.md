# LOG-001 Tuning Process

## Process

This process document describes the repeatable scalar tuning loop, the command shape, and the per-block decision flow for analyzer runs.

## Current scope

- Focus profile: `TonalPulseScalar`
- Goal: zero misses first, then tighten duration and hysteresis without bringing back duplicates
- Launch batch size: `50` trials per launch
- Working session target: about `30` minutes

## Device setup

- Flash the analyzer build before starting a tuning batch.
- Use `COM6` at `115200`.
- Keep `DTR` and `RTS` inactive.
- Ensure no other serial monitor is holding the port.

## Start command

Use the analyzer console command:

```text
SEQ start profile=TonalPulseScalar tries=50 mode=source when=all verbose=1
```

## Log layout

Each tuning batch gets its own folder under:

```text
logs/seq-tests/<timestamp>-log-001-tonalpulse-scalar/
```

Recommended files:

- `README.md`
- `session.log`
- `heartbeat.md`
- `campaign.lock`
- `campaign_state.json`
- `run_01.log` through `run_10.log`
- `block_01_summary.md` through `block_10_summary.md`
- `progress.md`

## Naming

Include the active scalar tuning values in the folder name so the run is self-describing.

Example:

```text
20260613_120000-log-001-tonalpulse-scalar
```

Suggested value order:

```text
maxDuration-onsetThreshold-releaseThreshold-releaseDebounce
```

## What to capture

Per launch, capture:

- `SEQ_TRIAL`
- `SEQ_SOURCE`
- `SEQ_SOURCE_SPEC`
- `SEQ_SUMMARY`

For the current scalar tuning focus, `mode=source when=all` is the preferred diagnostic shape.
It keeps the run centered on the selected source chain while preserving the full trial mix.

## What to check

For each block, look at:

- miss count
- duplicate count
- late count
- `avg_dt_ms`
- `avg_strength`

For misses, inspect:

- `SEQ_SOURCE`
- `SEQ_SOURCE_SPEC`
- `detail.scalar.inspect.reject_reason`

Also compare the requested `PARAM` line with the confirmed scalar snapshot from `PARAM STATUS`.
The block summary should document the applied tune, not just the intended one.

## Tuning rule of thumb

- First remove misses.
- Then tighten `maxTransientDurationMs` and hysteresis in small steps.
- Keep duplicates at `0` if possible.

## Current helper pattern

Use the scaffold helper (`tools/logging/+ create_log001_batch_scaffold.ps1`) to create the batch scaffold and store the log targets in one place.

The helper now writes a single-instance `campaign.lock` file and a small `campaign_state.json` snapshot so overlapping launches fail fast instead of corrupting the live batch.
If a second launch is rejected, the error message includes the owning PID from the state snapshot.
The helper also writes `heartbeat.md` so you can tell whether the runner is still alive even if the terminal goes quiet.
The helper records both the requested tune and the confirmed applied tune from `PARAM STATUS`.

The helper supports:

1. `Codex-run`
2. `User-run`

Interrupted batches can be resumed with:

```text
powershell.exe -File "tools/logging/+ run_log001_campaign.ps1" -BatchRoot <saved-batch-folder> -StartRun <next-run>
```

## Notes

- Do not depend on ad-hoc pasted serial text for tuning comparisons.
- Keep one folder per tuning variant.
- When a block is finished, summarize it before changing the next tuning value.
- If a batch ends early, inspect `session.log` for `campaign_failed` or `campaign_stopped_without_completion`, then compare with `heartbeat.md` and `campaign_state.json`.
