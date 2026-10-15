# 2026-10-15 5b gate battery: stepped hop vs the plain 3,200 Hz chirp, 60-110 cm (sessions 2026-10-15-gate-60cm ... -gate-110cm)

E2 Analyzer COM5 (512-bin history + band bank, band=2800-3800), E3 Emitter with hop (0.5 FS), boards facing up, quiet room,
10 trials per cell, default scalar params. Hop = 3 pulses of 30 ms, 10 ms gaps, 2800/3300/3800 Hz (`SEQ ... dur=30 pulses=3
gap=10 hop=2800,3300,3800 band=2800-3800 freq=3300`). Chirp = `SEQ ... freq=3200` (single bin, no band).

| Spacing | Hop accepted | Hop amp class / strength | Plain chirp accepted | Chirp amp class / strength |
|---|---|---|---|---|
| 60 cm | 10/10 | medium / 8,360 | 3/10 | 7 weak / 4,020 |
| 70 cm | 10/10 | medium / 6,908 | 10/10 | medium / 5,242 |
| 80 cm | 5/10 | 5 weak / 7,835 | 10/10 | medium / 6,006 |
| 90 cm | 0/10 | event only 85 ms (one of 3 pulses lost), 9 pattern_rejected | 9/10 | medium / 5,621 |
| 100 cm | 10/10 | medium / 8,183 | 0/10 | weak / 3,594 |
| 110 cm | 10/10 | medium / 9,522 | 10/10 | strong / 9,016 |

Neither signal alone reaches >= 9/10 at every spacing; they fail at different spacings (hop at 80 and 90 cm, chirp at 60 and
100 cm). At 90 cm a hop frequency lands in a comb null and the 3-step hop loses a third of its signal (85 ms event, at the
minimum duration). The plain chirp results at 70 and 80 cm differ from yesterday (0/10 then), so the comb moves with the day.
Silence per spacing not run (silence with the band default: 2026-10-15-band-noise).
