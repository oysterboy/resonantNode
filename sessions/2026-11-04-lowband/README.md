# 2026-11-04: does a hop train on a low band carry? (the owner asked for a band range of 300-5000 Hz)

All five as Nodes on 784f8dea (stored params as before, output.silent 1 on all). lowband.py: idle off, band per
node off on all; E1 silent 0 and RB CALL every 8 s, 10 calls per band; E2-E5 listen (silent: they detect, do not
answer). A fixed 5-bin band (detection.band_low_hz / high_hz), the train over its five bins. Desk layout as before.
PARAM LOAD on all at the end.

| band | train | listeners hearing each call (of 4) | strength median |
|---|---|---|---|
| 950-1950 (the tested reference) | 950 1200 1450 1700 1950 | 4 x 10 | 12,321 |
| 500-1500 | 500 750 1000 1250 1500 | 4 x 10 | 19,859 |
| 300-1300 | 300 550 800 1050 1300 | 4 x 10 | 25,819 |

The low trains carry on the desk, louder at the listeners than the 1 kHz reference (the input's DC blocker corners
at 100 Hz; the 4 ms window still parts the 250 Hz bins). Not tested: the room's low noise (music, voices, rumble)
against these windows. behavior.band_range_low_hz / high_hz accept 250 Hz since this session.
