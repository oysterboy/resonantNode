# 2026-10-28: step 11g bench gates, batch C (Analyzer A/B) and batch D (Node)

All five boards: before = 42f1eef (the 11h build), after = 184c72c (main with 11g B1, B2, C1-C5, D).

## Batch C: Analyzer A/B (tools/bench/analyzerab.py on E2 / COM3, Emitter E3 on its link)

before_42f1eef/ and after_184c72c/, one transcript per case. seqcmp.py on hop10, chirp3200_100, chirp3450_150: the
same trials, results and counts (hop10 10/10 both, chirps 0/10 both); avg_strength and avg_dt within the run-to-run
spread (13,925 / 13,689, 44 / 47 ms). Keys only in before: the ones 11g C3b pruned (avg_conf, ambiguous_trials,
too_dense_trials, inspect.integrity_queue_overflow, coverage.future_unavailable, integrity.queue_overflow,
detail.scalar.*.normalized and the other constant ones); only in after: dropped_dma_buffers in SEQ_SUMMARY (moved from
SEQ_HISTDBG). No other key and no common value changed. Edge cases:

| case | before | after |
|---|---|---|
| SEQ start during a run | second start accepted, the run discarded (2 starts) | `ERR SEQ start refused reason=run_active` (1 start) |
| SEQ start + SEQ STOP in one write | the run starts anyway (2 trials) | stopped, no trial |
| SEQ SUMMARY before the first trial ends | same lines | same lines |
| test= label of 69 characters | cut to 47 | cut to 47 |
| SEQ start + SEQ MODE quiet in one write | quiet, STATUS mode=quiet when=off | identical |
| SEQ STATUS idle | identical | identical |

## Batch D: Node (nodegateD.py / .out; E1 COM8, E4 COM10, E3 Emitter COM4)

- Boot timing on E1 after a reset: BUILD at 0.47 s, `RB warmup waiting` from 0.75 s, `RB warmup done ms=2000 rebase=ok`
  at 2.63 s, `RB baseline active` at 4.14 s (the quiet gate then took 1.0 s).
- 10 Emitter hop trains: 10 valid verdicts, 10 answers (output.silent 0 unsaved, back to 1 after).
- `RB debug events`: no reply (the command is gone; unknown RB commands are silent on the Node, review D1.9).
- E4 SELFTEST: board, mic, quiet_boot, amp PASS (git=184c72c). `SELFTEST external wait_ms=abc` prints the help
  (Kind Unknown) instead of running with 100 ms.
