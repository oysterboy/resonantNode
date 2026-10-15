# 2026-10-15 5b gate battery: 5-step hop (20 ms pulses, 2800/3050/3300/3550/3800 Hz, band 2800-3800) at 60-110 cm (sessions 2026-10-15-gate5-60cm ... -gate5-110cm, plus 2026-10-15-hop5-90cm)

E2 Analyzer COM5 (512-bin + band bank), E3 Emitter hop 0.5 FS, boards up, quiet room, 10 trials per cell. Gap 15 ms is the
owner's artistic wish; gap 5 ms the denser fallback. Default scalar params except where noted.

| Spacing | gap 15 ms | gap 5 ms | 3-step hop (30 ms x3, gap 10) | plain 3,200 Hz chirp |
|---|---|---|---|---|
| 60 cm | 9/10 (max_duration 300) | 0/10 | 10/10 | 3/10 |
| 70 cm | 10/10 (max_duration 300; 0/10 at default 200) | 10/10 | 10/10 | 10/10 |
| 80 cm | 0/10 (default max_duration; not rerun with 300) | 10/10 | 5/10 | 10/10 |
| 90 cm | 10/10 | 10/10 | 0/10 | 9/10 |
| 100 cm | 4/10 (6 amp weak) | 10/10 | 10/10 | 0/10 |
| 110 cm | 6/10 (4 amp weak) | 0/10 (amp weak) | 10/10 | 10/10 |

Where a cell fails with the detector accepting the event, the rejection is the evaluator with contrast class medium and amp class
weak (e.g. 110 cm, gap 5: contrast medium, amp weak, strength 8,522 - the highest strength of the day). The profile's amp inspector
(`AmpMagnitude`, strong/medium/weak peak thresholds 5000/2500/1000 in a 100 ms window around the peak) is calibrated for a
continuous chirp; 20 ms steps with gaps reach a lower amp peak. The detector side (band bank, FrequencyTarget strength) is not the
limit. No dropped buffers in these runs (buffer_overrun=0). Silence / speech / music for these hop shapes not yet run.
