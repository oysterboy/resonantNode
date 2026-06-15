# LOG-001 - Tuning Runs Logger + Param-Driven Scalar Campaign

Status: active

## Goal

Build a log-first tuning loop for `TonalPulseScalar` that uses the current `PARAM` surface and keeps the run shape reproducible from saved batch folders.

## Canonical run shape

```text
SEQ start profile=TonalPulseScalar tries=50 mode=source when=all verbose=1
```

## Implementation plan

1. Add a checked-in helper under `tools/` for the tuning batch scaffold.
2. Keep `PARAM` echoing only changed fields.
3. Keep `PARAM STATUS` printing the full active snapshot.
4. Keep scalar tuning names in snake_case.
5. Keep the seq-tests batch layout stable:
   - `session.log`
   - `run_01.log` through `run_10.log`
   - `block_01_summary.md` through `block_10_summary.md`
6. Capture `PARAM STATUS` before each block and after each parameter change.
7. Record misses, duplicates, late results, `avg_dt_ms`, and `avg_strength` in the block summaries.
8. Update the seq-tests docs so the stored workflow matches the new command shape.
9. Keep the campaign runner resumable from an existing batch folder via `-BatchRoot` and `-StartRun`.
10. Add a single-instance batch lock plus a tiny resume-state file so overlapping launches fail fast.

## Helper modes

- `Codex-run`: Codex or the helper can launch runs, read summaries, and apply the next `PARAM` shift.
- `User-run`: the helper prints exact commands and log targets for manual execution.

## Files to update

- `logs/seq-tests/seq-tests-guide.md` - guide
- `tools/logging/+ create_log001_batch_scaffold.ps1` - scaffold helper
- `tools/logging/+ run_log001_campaign.ps1` - campaign runner
- `tools/logging/tuning-run-process.md` - process doc
- `tools/logging/batch-readme-template.md` - batch template
- `tools/logging/log-001-process-notes.md` - process notes

## Test plan

1. Verify `PARAM STATUS` prints the full scalar snapshot.
2. Verify `PARAM` prints only changed fields.
3. Create a dry-run batch scaffold and confirm the expected file layout.
4. Confirm the canonical launch command is recorded as `mode source` plus `when all`.
5. Use the saved logs to compare miss count, duplicate count, late count, `avg_dt_ms`, and `avg_strength`.

## Acceptance

- The batch helper exists under `tools/`.
- The stored docs describe the `mode source` / `when all` workflow.
- The batch layout is reproducible from a saved folder.
- No detector semantics change is required for this pass.
