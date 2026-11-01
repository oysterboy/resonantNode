# 2026-11-01: step 22.3, the edge cases at band update 16

All boards on 02e6a24 (the bank every 16 samples) by OTA; desk at 79 cm; the Analyzer's band 2950-4700 (the tools'
new baseline). The Analyzer switches its interval live (`SEQ FREQUPDATEEVERYSAMPLES`), so the ladder interleaves 4 and
16 step by step (the room's comb drifts over hours; 2026-10-30-step17-soak8).

Margin ladder (ladder/, ladder_edge/; 10 trials a run):

| step | update 4 | update 16 |
|---|---|---|
| -3 dB (0.35 FS) | 20 / 20 | 20 / 20 |
| -6 dB (0.25 FS) | 20 / 20 | 20 / 20 |
| -7.5 dB (0.21 FS) | 50 / 50 | 43 / 50 (6, 10, 9, 10, 8) |
| hops 2 + 3 at -23 dB | 0 / 20 | 0 / 20 (pattern_rejected, both) |
| hops 4 + 5 at -45 dB | 20 / 20 | 20 / 20 |

The seven losses at -7.5 dB are all coverage_too_low: coverage above the release threshold 74-76 ms against the
minimum (80) where update 4 reads 83-84 ms on every trial. At the edge of a hop gap the coarser score loses a few ms
of coverage. The operating point and -6 dB are untouched.

Chord cell (chordcell/, answerers with level_max 0): alone / same / +50 / +150 one train per pair, +250 two, every
answer given: as at update 4 (2026-10-30-step14.2-boards).

Music (music_E*.log, bench_audio.wav once, every Node silent): 0 verdicts on E1 / E4 / E5.
