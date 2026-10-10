# 2026-10-10 issue #20 R4: noise false positives (D-AMP, 70 cm)

Setup: Emitter E3 (COM4, 24:dc:c3:49:b7:38, 965737a) -> Analyzer E2 (COM3,
24:dc:c3:4b:4c:e8), 70 cm, speaker faces mic, same height; tone 0.3 FS,
3200 Hz. Noise from the owner's phone ~50 cm from the Analyzer at normal
listening level; claps and a door by the owner. Office otherwise empty.
`SEQ DIAG off`, `mode=detail`, TonalPulseScalar.
Firmware: stock = main 246029b; variant = branch tune/issue20-r4 04f948f
(Scalar amp inspector strong/medium/weak 5000/2500/1000 -> 2000/1000/500,
decision 3 candidate; detector unchanged).

Silent OBS (50 windows x 1.8 s, no emission): a pattern-valid verdict is a
false positive (the node would answer it).

| Run | Noise | Firmware | Detector accepted | Pattern-valid (false positives) |
|---|---|---|---|---|
| OBS_speech_stock | speech | stock | 3 | 2 |
| OBS_music_stock | music | stock | 6 | 6 |
| OBS_claps_door_stock | claps + door | stock | 1 | 1 |
| OBS_music_variant | music | variant | 1 | 1 |

| Run | Noise | Firmware | Result |
|---|---|---|---|
| T3_speech_stock (50 chirps) | speech | stock | 48 expected, 2 rejected; avg strength 15,755 (15,781 in quiet) |

Reading:
- Stock TonalPulseScalar answers speech and music: 2 and 6 false positives
  in ~100 s each. The false positives look like the chirp to both
  inspectors (100-176 ms, contrast medium, amp medium/strong, strength
  4,200-8,200): a sound with energy near 3,200 Hz passes.
- Claps and a door are mostly rejected by duration/coverage (1 in 50).
- Speech barely masks the real chirp at 70 cm (48/50).
- Variant vs stock is not answered: the detector stage is identical in both
  builds, yet it accepted 6 candidates under music with stock and 1 with
  the variant, so the music content differed between the runs and
  dominates. Per candidate the variant inspector can only be more
  permissive. A fair comparison needs the same audio clip in both runs.

Not part of these runs: a one-block soak smoke test right after
OBS_music_variant gave T3 6/10 with 4 `duration_too_long` because the
phone was still playing; repeated in quiet: 10/10 (not committed, scratch).
