# LOG-001 Logging Guide

Status: active

## Guide

This is the front door for LOG-001 logging runs.
Use it to tell Codex what to run, or to run the campaign yourself with the helper scripts.

## What To Ask Codex

Tell Codex something like:

```text
Run LOG-001 for TonalPulseScalar.
Use 100 launches, 10 blocks, 50 trials per launch.
Keep mode=source and when=all.
Compare requested PARAM values against PARAM STATUS after each block.
Only shift one scalar knob at a time.
```

Codex will then:

- create or resume the batch folder under `logs/seq-tests/`
- flash the analyzer if the run needs a fresh binary
- open the serial port and execute the launches
- write `session.log`, `heartbeat.md`, `progress.md`, `run_XX.log`, and `block_XX_summary.md`
- compare miss count, duplicate count, late count, `avg_dt_ms`, and `avg_strength`
- use `PARAM STATUS` as the source of truth for the applied tune
- decide the next `PARAM` shift after each block

## Half Manual

If you want to do the run yourself, use the scaffold helper in `User` mode first:

```text
powershell.exe -File "tools/logging/+ create_log001_batch_scaffold.ps1" -Mode User -Profile TonalPulseScalar -TotalRuns 100 -BlockSize 10 -Tries 50
```

That prints the exact launch command and the folder targets.
From there you can:

- flash the analyzer build
- run the printed `PARAM STATUS` command
- run the printed `SEQ start ...` command in the analyzer console
- collect the serial output into the suggested batch folder
- decide the next parameter shift yourself after reading the block summary

If you want Codex to handle only part of the job, let Codex create the batch scaffold and summaries, then paste the commands into the analyzer yourself.
That is the easiest halfway point.

## Current Example

- Profile: `TonalPulseScalar`
- Default tuning target: `Scalar`
- Active command surface: `PARAM` and `PARAM STATUS`
- Launch shape: `SEQ start profile=TonalPulseScalar tries=50 mode=source when=all verbose=1`
- Run planning: 100 launches in 10 launch blocks
- Block size: 10 launches

## Batch Layout

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
- the confirmed applied tune from `PARAM STATUS`, not just the requested `PARAM`

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

Treat the `PARAM STATUS` line as the source of truth for the applied scalar tune.
If the requested tune and applied tune differ, the block summary should show both.

## More Detail

The operational run procedure, helper behavior, and resume flow live in
[tuning-run-process.md](./tuning-run-process.md).

## RAW PCM Campaign Note

For RAW PCM distance/angle campaigns, use PowerShell directly from the repo root.
Keep the capture command parameters unchanged unless the test plan says otherwise:

```powershell
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$batch = Join-Path (Get-Location) "logs\raw_pcm\50cm_45deg_$stamp"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "tools\logging\run_raw_pcm_capture.ps1" -BatchRoot "$batch" -TotalRuns 20 -PreMs 50 -PostMs 200 -Mode pcm -PauseBetweenRunsSec 0
```

Change only the campaign title prefix, for example `50cm_0deg_` or `50cm_45deg_`.
The helper writes one consolidated log at `$batch\session.log`, plus `heartbeat.md`,
`progress.md`, and `capture_state.json`.

Current RAW helper details:

- Run 001 sends the RAW command up to twice if the first attempt times out before `RAW_BEGIN`.
- Status-file writes retry briefly, so an open `progress.md` tab should not stop a campaign.
- A good completed run should show 20 `RUN ... COMPLETE` lines, 20 `RAW_BEGIN` lines, 20 `RAW_SUMMARY` lines, and 0 `RAW_ERR` lines.
- If the first run reports `RAW_ERR emitter_remote_claim_timeout`, check emitter power/control wiring and retry after COM3 opens cleanly.
