# 2026-10-22: step 10b, the step 6 protocol rerun on the refactored firmware (issue #50)

Setup:
- All five boards on Node **8ec0764** (main after step 10: evaluator folded, FrequencyMatch removed, live params,
  behavior.* params, RB BEHAV removed, Analyzer moved, glue split), shipped defaults.
- Layout as step 6 (owner's trial layout; E1, E4 and E5 10 cm lower than E2 and E3; mic and speaker facing up).
- Ports: E1 COM8, E2 COM3, E3 COM4, E4 COM10, E5 COM11. Run from the lab PC.
- `# state` headers carry PARAM DUMP from every node, now including the 13 behavior.* params (RB BEHAV is gone).

## 1. Hearing matrix (`tools/bench/hearmatrix.py --rest-ms 2000`)

| listener \ caller | E1 | E2 | E3 | E4 | E5 |
|---|---|---|---|---|---|
| E1 | - | 10 | 10 | 10 | 10 |
| E2 | 10 | - | 10 | 10 | 10 |
| E3 | 10 | 10 | - | 10 | 10 |
| E4 | 10 | 0 | 10 | - | 10 |
| E5 | 10 | 10 | 10 | 10 | - |

19 of 20 cells are 10/10 (step 6: 18). E4 <- E2 is 0/10 again (placement). E2 <- E1 is 10/10 (0/10 in step 6).
No cell is worse than in step 6. 0 answers.

## 2. Free run, quiet room, 30 min (`nodegroup.py --kick E1 --kick-every 90 --status-every 120`)

| | step 10b | step 6 |
|---|---|---|
| chains ended within 30 s | **41 of 41** (median 3.2 s, max 4.7 s) | 41 of 41 (max 5.2 s) |
| max emissions per minute (M = 100) | **40** | 42 |
| answers without another node's emission in the 2 s before | **0** of 724 | 0 of 757 |
| dropped DMA buffers / read errors / verdict-queue overflows | 0 / 0 / 0 in all 70 STATUS lines | 0 |
| resets | 0 | 0 |

## 3. Free run with speech and music, 30 min

Same as part 2, plus `bench_audio.wav` (C:\Users\admin\Downloads, 435 s: drum and bass, female speech, male
speech, tonal flute) played four times back to back with Windows SoundPlayer through the Studio One box at its
last volume (passes at 21:53:03, 22:00:18, 22:07:33, 22:14:49; done 22:22:04; playback_times.txt). Speaker
placement as in step 6 (midway, 40 cm off the line).

| | step 10b | step 6 |
|---|---|---|
| chains ended within 30 s | **40 of 40** (median 3.3 s, max 4.8 s) | 41 of 41 (max 4.9 s) |
| max emissions per minute | **45** | 45 |
| answers started by noise | **0** of 743 | 0 |
| invalid (rejected) verdict lines | 25 | 54 |
| dropped buffers / read errors / overflows / resets | 0 / 0 / 0 / 0 | 0 |

## Reading

**Step 10b's gate is met.** The refactored firmware behaves like the step 6 firmware: every chain ends within
5 s, the network stays under half of M, and no noise starts a chain. The hearing matrix is one cell better and none
worse.

Not covered: the "noise started" count uses the step 6 definition (an answer with no other node's emission in the
2 s before; the same script gives 0 on the step 6 log). Fewer invalid verdicts with the same file need not mean
anything: the speaker position was reproduced by eye.
