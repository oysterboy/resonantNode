# 2026-10-31: steps 16.3 and 17.3, five Nodes free on the desk (level automaton, 8-bin band, base bins, glide)

All five boards as Nodes on 13902862 for the session (E2 and E3 by OTA from the Analyzer / Emitter images and back
at the end, both confirmed). Every Node on the same values, saved: the level automaton's defaults (level_max 6,
decay 2 s, jitter 3 s shrinking, rest 8 s, the old rest triggers 0), max duration 450, band 2950-4700, output.silent 1
stored and 0 live for the runs. Base bins from the roster / saved: E1 0, E2 1, E3 2, E4 3, E5 1. Desk layout of
2026-10-26 (as 2026-10-22-step6-trial, Emitter-to-Analyzer 79 cm). Step 6's protocol; remote, the owner away.

## 1. Hearing matrix (1_hearing_matrix.log; hearmatrix.py now sets level_max 0 and the outputs on, live)

18 of 20 cells 10 / 10, 0 answers. E2 <- E1 0 / 10 (as on 2026-10-21 and in step 6), E5 <- E2 0 / 10 (step 6 had
E4 <- E2 instead): placement.

## 2. Free run, quiet, 30 min (2_free_run_quiet_30min.log, 2_summary.txt; nodegroup.py --kick E1)

- 296 emissions (196 answers, 80 idle calls, 20 RB CALLs); at most 19 per minute (M = 100: PASS).
- 80 chains, median 1.3 s, max 11.3 s, **80 of 80 ended within 30 s: PASS**.
- **0 answers without another node's emission in the 5 s before** (no spontaneous chains).
- Time to quiet (a 15 s silence within 30 s of a start): 43 of 100 starts, **FAIL against step 6's 80 %**. Not a
  chain that runs on: the starts are mostly idle calls (80, against 27 in step 6), and with the automaton a quiet
  node answers a lone call with 1 / 6, so most calls stay single and the next node's idle call comes before 15 s
  of silence (five nodes, 60 +- 20 s each).
- Rests: rest_level only (87); no other rest reason exists by default now.
- Base bins (tools/bench/basechains.py): 24 chains of six trains or more, distinct bases among the first three
  1.54 -> last three 1.08, **92 % settled on one base**; bases played 0: 53, 1: 117, 2: 118, 3: 8.
- Audio: E4 25 dropped DMA buffers, all before the first RB STATUS (at the run's start); none during the run, the
  others 0.

## 3. Free run with speech and music, 30 min (3_free_run_speech_music_30min.log, 3_summary.txt, 3_playback.txt)

As part 2, plus bench_audio.wav four times back to back from the lab PC (12:01:45-12:30:46, the Studio One box at
its standing level; speaker as in step 6).

- 285 emissions; at most 21 per minute: PASS. 88 chains, max 12.6 s, 88 of 88 ended within 30 s: PASS.
- **0 answers without another node's emission in the 5 s before: speech and music started no chain.**
- Time to quiet 42 of 113: FAIL, as part 2.
- Base bins: 23 long chains, 1.48 -> 1.26 distinct, 74 % settled.
- Invalid verdicts 20 (quiet run 66).

3x_free_run_no_music_player_failed_30min.log: the first try of part 3; the player script failed at once (the
quoting of its path), so it ran without music. Kept as a second quiet run: 75 chains all ended, 0 spontaneous,
60 % settled.

## Reading

Against 2026-10-22-step6-trial (the fixed answer and the old rest triggers): a third of the emissions (296 against
774), shorter chains (median 1.3 s against 3.3 s; max 11-13 s against 5 s), half the peak rate (19-21 against
42-45). Every chain ends, noise starts none. The glide is audible in the numbers: long chains end on one base.
The one mark missed, time to quiet, measures idle-call density under the automaton, not chain length: an owner
call whether it stays a mark (and if so, a longer idle_timeout for five nodes) or chains ended within 30 s
replaces it.
