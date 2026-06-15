# LOG-001 Batch Template

Status: planned

## Batch Template

This template is copied into each new LOG-001 batch folder and describes the expected snapshot, layout, and decision rule.

## Snapshot

- Profile: `TonalPulseScalar`
- Mode: `Codex-run`
- Launch command: `SEQ start profile=TonalPulseScalar tries=50 mode=source when=all verbose=1`
- Param snapshot command: `PARAM STATUS`
- Port: `COM6`
- Baud: `115200`
- Total launches: `100`
- Block size: `10`
- Blocks: `10`

## Log Layout

- `session.log` records the full batch session.
- `heartbeat.md` records the last live runner state.
- `campaign.lock` prevents overlapping launches.
- `campaign_state.json` stores the resume snapshot.
- `run_01.log` through `run_10.log` hold the block transcripts.
- `block_01_summary.md` through `block_10_summary.md` hold the block decisions.
- `progress.md` records the current block and current run while the batch is active.

## Decision Rule

- First compare miss count, duplicate count, and late count.
- Then compare `avg_dt_ms` and `avg_strength`.
- Compare the requested tune with the confirmed `PARAM STATUS` snapshot.
- Keep the current parameter values if the block regresses.
- If the batch stopped unexpectedly, treat `campaign_failed` and `campaign_stopped_without_completion` as the first clues.

## Workflow Modes

- Codex-run: Codex reads the saved logs and decides the next `PARAM` shift.
- User-run: the helper prints the exact commands and folder targets.

## Commands

```text
PARAM STATUS
SEQ start profile=TonalPulseScalar tries=50 mode=source when=all verbose=1
```

## Notes

- Keep the saved batch folder self-contained.
- Do not depend on pasted serial text alone.
- Update the parameter snapshot before and after each shift.
- Record the applied tune from `PARAM STATUS` in the block summary.
