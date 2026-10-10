# 2026-10-10 soak at 70 cm (#27, step 3a): aborted by a hardware fault

Setup: D-AMP Emitter E3 (COM4, 24:dc:c3:49:b7:38, 965737a) -> Analyzer E2
(COM3, 24:dc:c3:4b:4c:e8, 246029b), 70 cm, speaker faces mic, same height,
0.3 FS, 3200 Hz. Runner `tools/bench/soak.py` (625e8f9): every 15 min
10 x TonalPulseScalar T3 + 5 silent OBS windows, `SEQ DIAG off`,
`mode=detail`. Office empty from ~12:35. Started 12:17, stopped by hand at
20:19 (owner decision) after 33 of 96 blocks.

| Blocks | Time | T3 expected / 10 | median strength | drops | OBS detections |
|---|---|---|---|---|---|
| 1-5 | 12:17-13:17 | 10 each | 17,100 / 16,509 / 16,367 / 16,842 / 15,379 | 0 | 0 |
| 6-33 | 13:32-20:17 | 0 each | - | 0 | 0 |

From block 6 every trial is `unexpected / missing_pipeline_result`: the
detector saw no candidate at all. The link and the Emitter kept working
(`remote claim ... status=ok`, `emit_marker_seen=1` every trial), the
Analyzer read samples normally (27.6 us/sample, 0 dropped buffers, no
reset), and soak.py logged no errors (every run completed). So the chirp
stopped reaching the detector between 13:17 and 13:32 and never came back:
either E3's amp/speaker went silent or E2's mic stopped delivering data
(both failure modes seen on this bench on 2026-10-09/10). `SEQ DIAG off`
carries no level data, so the logs cannot tell which; the next bench
session runs the amp check / SELFTEST first.

Not an answer to the NODE-013 gate: 5 good blocks (1 h). Blocks 1-5 spread:
median strength 15,379-17,100 (about +/-5%) within one hour.

Lesson: a run that only counts detections cannot tell "nothing heard" from
"nothing played". soak.py should flag a block with 0 detector candidates as
an error, and the next soak starts only after SELFTEST (#28) passes on both
boards.
