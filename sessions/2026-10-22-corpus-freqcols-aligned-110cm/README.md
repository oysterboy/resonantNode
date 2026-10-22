# 2026-10-22: step 7b, RAW captures with the board's frequency path on the network band, host vs board

Setup:
- E2 (COM3): Analyzer, main + 7b RAW changes. E3 (COM4): Emitter at 0.5 FS. 110 cm apart, trial layout.
- E1/E4/E5 held silent.
- `tools/bench/rawcorpus.py --band 2950-3950`: 3 chirps of 3,200 Hz and 3 hop trains of 25 ms x 5, gap 10.
- RAW mode=both writes ms,amp,pcm,freq,contrast,fresh. RAW_BEGIN carries band= and band_lead=.

Host replay `--compare`, all six captures:

| stream | host vs board |
|---|---|
| amp (Strength16) | max 1 |
| freq (target-band FrequencyScore16) | max 1 |
| contrast (band contrast, where it is >= 1000) | max 0.5 %, mean < 0.01 % |

Three sessions are needed to read this:
- `2026-10-22-corpus-freqcols-110cm`: RAW had no band= yet, so the board ran single target. These captures match
  only with `--band off`.
- `2026-10-22-corpus-freqcols-band-110cm`: band=2950-3950, but the band stream was not reset at capture start. The
  board's round-robin bin updates ran in another phase than the host's, so freq was off by ~45 on average and
  contrast by ~2.5 %. That was alignment, not maths.
- this folder: the band stream is reset at capture start, and the replay pads band_lead % update_every. The values
  match exactly.

Two captures are fixtures (`test/fixtures/*_band_freqcols_110cm_2026-10-22.csv`), and
test_replay_corpus `test_host_frequency_path_matches_the_board_on_the_band` checks them.
