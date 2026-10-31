# 2026-10-31: step 19, the hop train around 1 kHz (owner: "Bitte auch noch 1khz testen")

Analyzer E2 / Emitter E3 on 1f0927a7, Nodes E1 / E4 / E5 on 13902862; desk layout of 2026-10-26 (79 cm), 0.5 FS,
remote. The question: can a band per node wander down to ~1 kHz (E004, 2026-10-27: below 1250 Hz music reached
the detector and 40-60 % of trains were lost under noise, with the 5-bin band then shipped).

Quiet, Analyzer, 10 trials each (quiet_summary.txt):

| case | result | mean strength |
|---|---|---|
| hop10 on 700-1700 | 10 / 10 expected | 31,978 |
| hop10 on 950-1950 | 10 / 10 expected | 21,911 |
| hop10 on 1450-2450 | 10 / 10 expected | 6,715 |
| chirp 1200 Hz / 100 ms on 950-1950 | 10 / 10 rejected (shape) | |

(For scale: hop10 on 2950-3950 reads ~14,000-14,700 at the same level; the speaker and the room are louder low.)

With speech and music, Analyzer (hop10_950_music.log, music_summary.txt): 90 trials on 950-1950 while
bench_audio.wav played once (15:52:13-15:59:29): 85 expected, 4 lost (the music opened an occurrence just before the
train: trials 47, 51, 67, 89, mostly in the male-speech section), 1 rejected (contrast weak, trial 53). No dropped
DMA buffers.

With speech and music, Nodes (nodes_music_E*.log): E1 / E4 / E5 silent, idle off, band 950-1950 live (PARAM LOAD
after), the file once (16:01:45 on): 16 / 21 / 0 verdicts, **0 valid** on every Node: the music forms candidates at
1 kHz, the shape rule rejects them all, nothing would start a chain.

Reading: around 1 kHz the train is heard at least as well as at 3-4 kHz (louder), the shape rule keeps music out
of valid verdicts, and the cost under music is ~5 % of trains lost (against 0 at 2950-3950 in the soaks). The 2026-10-27
loss under noise (E004) does not repeat with the shape rule in place.
