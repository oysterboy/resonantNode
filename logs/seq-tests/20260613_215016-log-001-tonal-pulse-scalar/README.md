# LOG-001 Batch Run

Status: running

## Snapshot
- Profile: `TonalPulseScalar`
- Mode: Codex-run
- Launch command: `SEQ start profile=TonalPulseScalar tries=50 mode=source when=all verbose=1`
- Param snapshot command: `PARAM STATUS`
- Port: `COM6`
- Baud: `115200`
- Total launches: `100`
- Block size: `10`
- Blocks: `10`

## Log Layout

- `session.log` records the full serial session.
- `heartbeat.md` records the last live runner state.
- `campaign.lock` prevents overlapping batch runs.
- `campaign_state.json` records the current resume state.
- `run_01.log` through `run_10.log` capture the block transcripts.
- `block_01_summary.md` through `block_10_summary.md` capture the block decisions.
- `progress.md` is updated while the campaign runs.
- If the batch is resumed, earlier completed runs stay in place and only the incomplete suffix is rewritten.
- If the batch stops unexpectedly, check `session.log`, `progress.md`, `campaign_state.json`, and `heartbeat.md` together.

## Tuning Ladder

- Block 1: baseline snapshot.
- Block 2: shorten max duration first.
- Block 3: lower release debounce a little.
- Block 4: continue duration tightening.
- Block 5 and later: lower onset / release thresholds gradually if the earlier blocks stay clean.

## Notes

- The campaign uses the current scalar snapshot unless a block decision changes it later.
- Keep the saved batch folder self-contained.
