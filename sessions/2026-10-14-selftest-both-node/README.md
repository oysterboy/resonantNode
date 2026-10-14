# 2026-10-14 SELFTEST, both boards on the Node build (7882627)

E2 (COM3, 24:dc:c3:4b:4c:e8) and E3 (COM4, 24:dc:c3:49:b7:38) both flashed
with `esp32dev` (Node, D-AMP) at 7882627. Runner: `selftest.py --ports COM3
COM4 --reset --external`. E2 had been unplugged/replugged several times.

| Board | mic | quiet_boot | amp | own_chirp | external_chirp |
|---|---|---|---|---|---|
| E2 | PASS floor 1102 | PASS | PASS 52.7 dB, tone 477k | PASS | PASS (helper E3) |
| E3 | PASS floor 1276 | PASS | PASS 56.4 dB, tone 839k | PASS | SKIP no_external_chirp (helper E2) |

E2's own tone is back at 477k (the -8 dB / 182k of 2026-10-12 skipfix run 6
is gone after the re-plugging, so that was seating). E3 heard 5 verdicts
during E2's test chirps but none valid: E2 -> E3 is the weaker direction
(E3 -> E2 passes). Not diagnosed; placement/orientation of the pair was not
recorded.
