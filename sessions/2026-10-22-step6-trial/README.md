# 2026-10-22: step 6, the five-node trial on D-AMP (issue #16)

Setup:
- All five boards on Node 908dc9a, the shipped defaults:
  - 0.5 FS, hop train 25 ms x 5 with gap 10 on the band 2950-3950;
  - TonalPulseScalar with the shipped thresholds;
  - rest 5 s; idle call 60 s +- 20 s.
- Logs carry `# state` lines (RB BEHAV and PARAM DUMP) from every node.
- Layout (owner): E2 and E3 110 cm apart; E1, E4 and E5 left of E3, 40 cm apart, starting 40 cm from E2; E1, E4 and E5
  10 cm lower than E2 and E3. Mic and speaker facing up, on the bench.
- Ports: E1 COM8, E2 COM3, E3 COM4, E4 COM10, E5 COM11 (E5 on its 5 m+ repeater cable).
- Run from the lab PC; the owner was away.

## 1. Hearing matrix (first part, `tools/bench/hearmatrix.py`)

Each node played 10 RB CALLs; the listeners rested instead of answering. There were 0 answers.

| listener \ caller | E1 | E2 | E3 | E4 | E5 |
|---|---|---|---|---|---|
| E1 | - | 10 | 10 | 10 | 10 |
| E2 | 0 | - | 10 | 10 | 10 |
| E3 | 10 | 10 | - | 10 | 10 |
| E4 | 10 | 0 | 10 | - | 10 |
| E5 | 10 | 10 | 10 | 10 | - |

18 of 20 cells are 10/10, and every listener hears at least three of four callers. No board fails its row, so none
is swapped. E2 <- E1 was 0/10 on 2026-10-21 too; E4 <- E2 was 10/10 then. The weak pairs are placement, not a
board.

## 2. Free run, quiet room, 30 min (`2_free_run_quiet_30min`)

`nodegroup.py --kick E1`: RB CALL every 90 s; RB STATUS every 120 s from every node (14 each).

- 774 emissions (757 answers, 27 idle calls, 20 RB CALLs); 16-42 per minute.
- 41 distinct chains: median 3.3 s, max 5.2 s, **41 of 41 ended within 30 s: PASS**.
- Max 42 per minute against **M = 100: PASS**.
- The 15 s silence came after 40 of 47 starts.
- 132 rests (rest_field_dense 100, rest_own_budget 32).
- **0 answers without an emission in the 2 s before** (no spontaneous chains).
- Audio health: `i2s.dropped_dma_buffers=0` and `read_errors=0` in all 70 STATUS lines.
- E5 heard throughout (211 verdicts).

## 3. Free run with speech and music, 30 min (`3_free_run_speech_music_30min`)

Same as part 2, plus `bench_audio.wav` played four times from the lab PC through the Studio One box (01:42-02:11,
the 2026-10-21 level). Each pass has music (drum and bass), female speech, male speech and a tonal flute, 100 s each.
The speaker stood midway between the nodes and 40 cm off their line.

- 815 emissions; 14-45 per minute.
- 41 distinct chains: median 3.7 s, max 4.9 s, **41 of 41 ended within 30 s: PASS**.
- Max 45 per minute, **M = 100: PASS**.
- The 15 s silence came after 39 of 48 starts.
- **0 answers without an emission in the 2 s before: speech and music started no chain.**
- 54 invalid (rejected) verdicts against 39 in the quiet run: the noise forms candidates, and the inspectors reject
  them.
- No dropped DMA buffers.

## Reading

**Step 6's gate is met.** It asked for three things in one recorded five-node session:
- STATUS from every node;
- the hearing matrix;
- the third question as counts, against the 2026-10-22 pass mark.

The network settles after every start, about 4 s per chain. It stays under a third of M. In 60 minutes, with and
without speech and music, it started no chain from noise.

Not covered (later NODE-012 cells, step 5h):
- another room;
- a dry cell;
- a height-offset cell beyond this layout's 10 cm;
- a full day of running.
