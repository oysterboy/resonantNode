# LOG-001 Batch Scaffold

Status: planned

## Snapshot
- Profile: `TonalPulseScalar`
- Mode: `Codex`
- Launch command: `SEQ start profile=TonalPulseScalar tries=50 mode=source when=all verbose=1`
- Param snapshot command: `PARAM STATUS`
- Port: `COM6`
- Baud: `115200`
- Total launches: `20`
- Block size: `2`
- Blocks: `10`

## Log Layout

- `session.log` records the full batch session.
- `campaign.lock` prevents overlapping launches.
- `campaign_state.json` stores the resume snapshot.
- `run_01.log` through `run_10.log` hold the block transcripts.
- `block_01_summary.md` through `block_10_summary.md` hold the block decisions.

## Decision Rule

- First compare miss count, duplicate count, and late count.
- Then compare `avg_dt_ms` and `avg_strength`.
- Compare requested tune against the confirmed `PARAM STATUS` snapshot.
- Keep the current parameter values if the block regresses.

## Tuning Ladder

- Block 1: baseline snapshot.
- Block 2: shorten `scalar_max_duration_ms`.
- Block 3: lower `scalar_release_debounce_ms`.
- Block 4: tighten duration again.
- Block 5 and later: lower thresholds gradually if the earlier blocks stay clean.

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
