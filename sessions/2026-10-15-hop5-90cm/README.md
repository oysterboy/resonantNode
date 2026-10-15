# 2026-10-15 5b: denser 5-step hops at 90 cm (the spacing where the 3-step hop failed 0/10), band 2800-3800, default params

E2 Analyzer COM5 (512-bin + band bank), E3 Emitter hop 0.5 FS, boards up, quiet room, 10 trials per cell. Hop
2800,3050,3300,3550,3800 Hz (5 pulses; `SEQ ... pulses=5 dur=PULSE gap=GAP hop=... band=2800-3800 freq=3300`).

| pulse / gap (ms) | event span programmed | result | detail |
|---|---|---|---|
| 20 / 5 | 120 | **10/10** | amp medium, 156 ms, strength 7,071 |
| 30 / 5 | 170 | 3/10 | 7 duration_too_long (204 ms, default max 200) |
| 25 / 10 | 165 | 2/10 | 8 duration_too_short (75 ms seen) |
| 20 / 15 (owner's artistic wish: 15 ms gaps) | 140 | **10/10** | amp medium, 127 ms, strength 7,089 |
| 15 / 15 | 135 | 0/10 | coverage_too_low, strength 5,958 |
| 25 / 15 | 185 | 4/10 | 6 duration_too_short (81 ms seen) |

20 ms pulses work with 5 and 15 ms gaps. 15 ms pulses carry too little energy per step; 25-30 ms pulses make the train long
enough that the single-chirp duration limits (min 85, max 200) and the event splitting interfere. Reference at 90 cm the same
day: 3-step hop (30 ms x3, gap 10) 0/10, plain 3,200 Hz chirp 9/10 (2026-10-15-gate-90cm).
