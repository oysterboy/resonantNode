# 2026-10-15 OUT-005 step 2: Emitter hop (PULSES hop=F1,F2,... one frequency per pulse)

E2 Analyzer COM5 (512-bin history), E3 Emitter COM3 with the hop renderer (0.5 FS). Train: 3 pulses of 15 ms, gap 20 ms
(`SEQ start ... dur=15 pulses=3 gap=20 hop=...`). The detector watches one fixed band (`SEQ freq=` sets the target), with relaxed
params (min duration 10, island 0, coverage 0) so a single pulse can register. If the hop plays three frequencies a target sees
about one pulse.

| hop | target | event the detector sees (median) |
|---|---|---|
| 3200,3300,3400 (100 Hz steps) | 3200 | 63 ms (4 of 10 expected) |
| | 3300 | 59 ms |
| | 3400 | 75 ms |
| 2800,3300,3800 (500 Hz steps) | 2800 | 27 ms, island 15 |
| | 3300 | 38 ms, island 29 |
| | 3800 | 20 ms, island 20 |

With 100 Hz steps each target sees about two pulses' worth: the band stream's Goertzel window (64 samples = 4 ms, ~250 Hz bin
width) cannot separate steps that close. With 500 Hz steps each target sees roughly one pulse, so the three pulses are at three
frequencies. The ack in the Emitter log was not captured. Detection of a hop as one signal needs the band detector (DET-013).
