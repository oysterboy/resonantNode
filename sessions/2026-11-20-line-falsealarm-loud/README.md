# 2026-11-20 night: loud noise on the line of four, nobody calling

falsealarm.py at the end of the night (05:03-05:44): Downloads/bench_audio.wav through the studio speaker, all four
nodes listening, window 2500-3500, --listen band, 20 min per preset.

| Preset | R1 | E2 | E4 | E3 |
| --- | --- | --- | --- | --- |
| trill-taper, valid (false calls) | 12 | 1 | 3 | 0 |
| trill-taper-glide-grain-risefall | 0 | 0 | 1 | 0 |
| peak level (trill-taper block) | 7554 | 8111 | 4543 | 3498 |

R1 and E2 stand nearest the speaker. trill-taper (no shape check) lets through 16 in 20 min across four nodes,
-risefall 1. Read with the maps: ../2026-11-20-line-map-trill-taper/README.md.
