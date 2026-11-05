# 2026-11-05 Node listening: presets, then the signal alone

All five on esp32dev a7d6715c (E2 COM3, E3 COM4 logged; E1, E4, E5 on a power-only charger, set by NET * from E2,
scratchpad radiopreset.py = nodepreset.py's lists sent locally and as NET *). Presets live, not saved:
low (low.log), deep (deep.log: 300-2000 Hz fits a 1000 Hz window in three places only, 250 Hz steps), wide (wide.log).
The owner then asked to hear the signal alone, behavior later: E2 only, the others output.silent 1.

chirptour.py (chirptour.log): the train (5 x 25 ms, gap 10, climbing 250 Hz a step) at every window 250-1250 ..
6750-7750 Hz, then shapes at 2000-3000. chirptop.py: the top half again, top down (6750 .. 3250).
chirpshort3.py: short and gliding shapes, PC beeps n times before variant n, each played twice.

Owner's verdicts:
- positions: "1 and [?] out" (the lowest; which second one not settled); the top end: listened again, no verdict yet
- shapes: 50 ms and 100 ms pulses out; gliding interesting, shorter tends to be better
- short round: 1 (8 x 10 ms, gap 0) yes, 2 (8 x 15, 0) no, 3 (8 x 25, 0) maybe, 4 (5 x 10, 0) yes, 5 (5 x 15, 0)
  maybe, 6 (5 x 10, gap 5) yes, 7 (3 x 10, 0) maybe, 8 (3 x 15, gap 5) yes
Not checked: whether the detection hears these shapes (the shape rule wants >= 3 band bins with 2 steps up; 10 ms
pulses against the 4 ms window): a detection pass of its own before any becomes the default.
All boards silent at the end (output.silent 1 live; stored values unchanged).
