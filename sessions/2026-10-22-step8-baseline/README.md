# 2026-10-22: step 8 baseline (3d check 12), before the evaluator fold

Setup: trial layout. E2 (COM3) Analyzer on main 3a11d80 (src identical to 518ffe6), E3 (COM4) Emitter 23d034a,
110 cm. E1/E4/E5 held silent by another session's open ports. 13:45-13:49.

## T3 on hardware (tools/bench/seqrun.py, detail + diag)

| run | result | durations (ms) | dropped DMA buffers |
|---|---|---|---|
| T3_chirp50 | 50/50 expected | 117-120 (39), 127 (1), 141-142 (10) | 0 in 50 |
| T3_hop10 | 10/10 expected | 196-197 | 0 in 10 |

Durations move with the room through the day: at 11:50 (2026-10-22-7d-freq-removed) the 100 ms chirp read 126-128
ms in all 20 trials; now it reads in two groups ~22 ms apart. Same mechanism as the 40 ms finding
(2026-10-22-7d-dur40-raw): a late reflection that sometimes stays above the release threshold. Compare durations
with this spread in mind.

## T2h host replay (deterministic)

- `t2h_replay.518ffe6.txt`: `native-replay --verbose --dir` over test/fixtures and the ten bench/sessions/*-corpus-*
  folders, built from HEAD (Scalar only). 120 captures, 80 verdict lines.
- `t2h_replay.pre7d-23d034a.txt`: the same with the replay binary built before the removal (23d034a).
- The two are byte-identical, so 7d passes T2h too. Step 8 diffs against `t2h_replay.518ffe6.txt`.
