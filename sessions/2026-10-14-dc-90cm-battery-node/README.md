# 2026-10-14 step 5a item 4 (#35): Scalar battery on the Node build, DC blocker, 90 cm, up, 0.5 FS

E2 Node build (default preprocessor: DC blocker 100 Hz x0.588, code bfbe0c8), E3 Emitter 0.5 FS,
3,200 Hz, boards facing up, 90 cm, window and doors closed, Roomba off. tools/bench/nodecount.py
counts Emitter chirps and the Node's `RB verdict=` lines over 100 s (RB log full). In the
silence / speech / music runs the Emitter is set to REMOTE (silent). The Node was not reset
between the runs (the port open did not reset it), so it stayed ACTIVE from the first run's
baseline. The `BEH verdict ... source=idle` lines and `node_emit_starts=5` in the quiet runs
are the Node's own idle chirps (20 s +- 5 s), not answers.

| Run (100 s) | Emitter chirps | Node verdicts | valid | Node answers |
|---|---|---|---|---|
| N_chirps_100s | 50 | 50 | 50 | 50 |
| N_silence_100s (radio off) | 0 | 0 | 0 | 0 (5 idle chirps) |
| N_speech_100s (WDR5, 50 cm, full volume, 90 deg) | 0 | 0 | 0 | 0 (5 idle chirps) |
| N_music_100s (drum and bass, same setup) | 0 | 0 | 0 | 0 (5 idle chirps) |

Analyzer half of the battery: 2026-10-14-dc-90cm-battery (T3 50/50) and 2026-10-14-dc-90cm-noise
(silence / speech / music, 50 windows each, 0 valid). Node selftest: 2026-10-14-selftest-node-dc-e2-radio-off.
