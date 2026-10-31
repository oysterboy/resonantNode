# 2026-10-31: the band bank's update interval on the host replay (owner: "every 4 samples, maybe too much?")

Host only. `native-replay --band-update N` (new, 2026-10-31) over the T2h set (every committed corpus folder, as in
2026-10-30-step17-band8), N = 4 (the boards' value), 8, 16, 32 samples (0.25 / 0.5 / 1 / 2 ms at 16 kHz; the
Goertzel window is 64 samples, 4 ms). t2h_upd<N>.txt; t2h_upd4.txt is byte-identical to t2h_replay.3e5ca34.txt.

| N | verdicts with the same valid / failed target | start shift max | duration shift max | strength change max |
|---|---|---|---|---|
| 8 | 81 / 81 | 0 ms | 15 ms | 1.7 % |
| 16 | 81 / 81 | 0 ms | 15 ms | 2.9 % |
| 32 | 81 / 81 | 1 ms | 15 ms | 6.6 % |

Every folder keeps its verdict and valid counts at every N. Only the measured values move: strength (the band
score is sampled less often, so the peak is caught less exactly), one occurrence's duration by 15 ms.

Caveats: 81 verdicts from 60-110 cm hop trains and chirps and one noise capture; no capture at the margin's edge
(-7.5 dB, two hops lost), of chords or of the music file, where a few % of strength or a shifted release could flip
a verdict. Not measured: what the loop saves (the bank's cost per update times the updates saved); that needs the
board's loop timing. Changing the boards' value is a detection change for its own pass (T2h re-recorded, the
Analyzer's margin set, a soak), not done here.
