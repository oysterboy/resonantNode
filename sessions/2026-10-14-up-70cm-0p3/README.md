# 2026-10-14 step 5a (#35): upwards geometry, 70 cm, Emitter tone 0.3 FS (Scalar, main c34fee9)

Stock Emitter (0.3 FS), E2 Analyzer, both facing up, 70 cm, window and doors
closed, Roomba off. SELFTEST (2026-10-14-selftest-up-70cm-0p3-gate): both PASS,
E3 own tone -5.0 dB vs the 0.5 FS gate there.

| Run | Result |
|---|---|
| T3_scalar (50) | 0 expected, 50 rejected (all unknown classes = source stage); duration 38-67 ms (median 45); avg strength 3,509 |

Compare 70 cm at 0.5 FS (2026-10-14-up-70cm-0p5): durations 50-74 ms, strength
5,800-6,200 in the same quiet room; and 110 cm: 0.3 FS 4,190 / 27 of 50,
0.5 FS 6,965 / 50 of 50. At 70 cm the received strength is no higher than at
110 cm (3,509 vs 4,190 at 0.3 FS), and a lower level gives shorter detections,
so the level is not too high at 70 cm. Not proven why 70 cm is weaker (reflection
geometry of the upwards layout is the candidate). Speech and music not run.
