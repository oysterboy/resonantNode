# 2026-10-16 capture corpus, 60 cm: single tones at the hop bins (tools/bench/rawcorpus.py)

RAW captures (`RAW trigger ... mode=both`: PCM plus the detector's amp feature, 50 ms pre, 250 ms post), each as the serial transcript (`.csv`), a 16 kHz 24-bit mono WAV and an entry in `captures.json` (RAW_BEGIN fields, firmware banner, tone check). E2 Analyzer COM5 on the combined build (162c847 + overrides, see 2026-10-16-gate-combined-60cm), E3 Emitter 0.5 FS, 100 ms tones, mic and speaker facing up. Purpose: the comb at the frequencies the hop uses, at the gate placements, for an offline replay (NODE-006) and as regression data. pos0 = the placement of the morning gate at 60 cm; pos1 = E3 moved to 62 cm.

Tone amplitude (single-bin DFT over the emit window, two captures each):

| f (Hz) | pos0, 60 cm | pos1, 62 cm |
|---|---|---|
| 2800 | 13,555 / 13,845 | 10,524 / 10,519 |
| 3200 | 23,810 / 20,941 | 38,917 / 34,570 |
| 3300 | 20,519 / 19,645 | 18,520 / 19,404 |
| 3800 | 5,337 / 5,192 | 35,748 / 30,040 |

3,800 Hz is in a null at 60 cm (a quarter of the other tones; the morning 3-step hop lost a step here, 6/10) and seven times stronger 2 cm further: the comb in numbers. Companion folders: 2026-10-16-corpus-{70,80,90,100,110}cm, -corpus-noise.
