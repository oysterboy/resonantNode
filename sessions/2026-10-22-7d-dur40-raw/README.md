# 2026-10-22: why a 40 ms tone reads 86 ms (follow-up to 2026-10-22-7d-freq-removed)

Setup: as 7d (E2 Analyzer 3a11d80, E3 Emitter 23d034a, 110 cm, trial layout). `tools/bench/rawcorpus.py`, mode=both,
band off: 3 x 40 ms and 2 x 100 ms at 3200 Hz.

## Result

- The Emitter plays 40 ms: the PCM envelope at the mic is 40-45 ms (rawcorpus `env`).
- The host replay of the same PCM reads 86 ms, the same as the board's SEQ (missing_support at the contrast gate).
  The extra time is in the sound that arrives, not in the firmware.
- The FrequencyTarget feature after onset, max per 5 ms (release threshold 1500, attack 2000):

  ```text
  0-50 ms   2,700 -> 5,500 -> 2,900   the tone
  55-80 ms  2,050 - 2,520             a late plateau (reflection / room tail), 30-40 ms after the tone ends
  85 ms     1,610
  90+ ms    < 1,200
  ```

  The plateau sits 550-1,000 above the release threshold, so the candidate stays open until ~85 ms. 7a read
  61-62 ms (9 of 10) and one 87 ms: there the plateau was mostly just below 1,500. The 100 ms tone carries the
  same plateau after its end (105-150 ms at 1,800-2,600).

## Reading

A short tone's duration flips between ~62 and ~87 ms depending on whether a late reflection clears the release
threshold. The strength is up 2.6 % since 7a (5,291 -> 5,427), which is enough. What raised it (temperature,
humidity, something moved, the Emitter's level) is not recorded. The duration gate (85 ms) is not what keeps
40 ms tones out in this room; the contrast inspector is. This is audit 3.1's 85-89 ms window (step 8b).
