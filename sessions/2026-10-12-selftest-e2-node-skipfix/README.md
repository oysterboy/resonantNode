# 2026-10-12 SELFTEST dead-mic SKIP fix on hardware (#28, bd41423)

E2 (COM3, 24:dc:c3:4b:4c:e8) flashed with the Node build `esp32dev` at
bd41423 (replaces the Analyzer build it ran in the soak). E3 (COM4, Emitter)
untouched, idle in REMOTE. Runner: `selftest.py --ports COM3 --reset`.

| Run | Wiring | mic | quiet_boot | amp | own_chirp | summary |
|---|---|---|---|---|---|---|
| 1 | normal | PASS floor 455 | PASS | PASS 53.0 dB, tone 454k | PASS 0 own verdicts | PASS |
| 2 | amp DIN (GPIO 32) pulled, by mistake | PASS floor 433 | PASS | FAIL low_snr -6.5 dB, tone 424 | **SKIP amp_failed** | FAIL |
| 3 | a mic wire pulled, not the data line | PASS | PASS | PASS 53.6 dB | PASS | PASS |
| 4 | mic SD pulled at the mic end | PASS floor 427 | PASS | PASS 54.3 dB, tone 466k | PASS | PASS |
| 5 | **GPIO 33 pulled at the ESP32 header** | **FAIL all_zero** | **SKIP mic_failed** | **SKIP mic_failed** | **SKIP mic_failed** | FAIL |
| 6 | all wires back | PASS floor 937 | PASS | PASS 45.8 dB, tone 182k | PASS | PASS |

Run 5 is the gap from 2026-10-11 (sessions/2026-10-11-selftest-neg-mic,
be61884: mic FAIL all_zero, but amp PASS 119 dB on TX pickup at floor 0):
with bd41423 the dependent checks now SKIP instead of passing vacuously.
Run 2 exercises the amp_failed half. The Node prints quiet_boot after the
mic check (new order in every log).

Run 4: pulling the wire at the mic end left the mic fully working (same
floor and own-tone level as runs 1/3), so on E2 the SD line reaches GPIO 33
by a second path at that end; only the ESP32-side pin disconnects it. Worth
knowing for future negative controls on this board.

amp valid=4 of 5 chirps in every run (one trial invalid each time); the
median is over the 4.

Run 6: PASS, but the own tone is 182k vs 454-467k in runs 1/3/4 (-8 dB;
the runner did not flag it DEGRADED). The wiring was handled several times
in between; check E2's amp DIN / speaker seating and re-run before E2 is
used for a level-sensitive measurement.

