# HW-005 on R1: the node's own radio against its mic (2026-11-13, 17:57-18:34)

Issue #99. R1 (stripboard board 1, `lochraster_v2`), image `esp32dev-lr2` at ba8bcac2 (flashed 17:52, the committed
light code), USB + buck, speaker on the box, mic ~10 cm above. E0 gateway on COM12. E1-E3 muted by radio (not
saved). Owner away from the bench; fan at 1 m running; room otherwise quiet. R1's call off, `RB log full` (every
onset prints an `RB verdict=` line). Script `hw005.py` (this folder), raw `hw005.json`, `console.log`.
Opening R1's port restarts it; the script used that as the reboot.

## Listening under traffic (radio on, strip dark, 5 min each)

| phase | traffic from E0 | onsets (verdict lines) | valid | i2s drops | tx_failed |
|---|---|---|---|---|---|
| quiet 1 | none | 0 | 0 | 0 | 0 |
| traffic | 154 `NET R1 RB STATUS`, 462 reply frames from R1 | 1 | 0 | 0 | 0 |
| quiet 2 | none | 0 | 0 | 0 | 0 |
| radio off, quiet | radio off | 0 | 0 | 0 | - |

The one onset under traffic: `single_pulse`, 227 ms, strength 11637, rejected (`ignored_invalid_pattern`). One in
5 min against none in 15 min is not a rate; its 227 ms length looks like a sound in the room more than a TX burst
(not attributed).

## SELFTEST sets (10 each, USB)

| radio | strip | traffic | amp PASS | median SNR per test, dB | floor median | floors > 6k |
|---|---|---|---|---|---|---|
| on | cap | none | 10/10 | 32.6-40.8 | 2261 | 6707 |
| on | cap | 90 req / 531 frames | 10/10 | 33.4-39.4 | 2739 | - |
| on | dark | none | 10/10 | 35.0-42.2 | 1883 | - |
| on | dark | 85 req / 511 frames | 10/10 | 37.5-42.5 | 1986 | - |
| off | cap | - | 10/10 | 35.7-41.0 | 2020 | - |
| off | dark | - | 10/10 | 35.0-40.5 | 2120 | 8763 |

The short trial before this run (one test per set, same image) had floors of 15425 (radio on, no traffic, dark)
and 14333 (radio off, cap, chirp 1).

## Reading

- The radio is not the cause of the single-chirp floor spikes: they appear with the radio off and without traffic,
  and none appeared in the two traffic sets.
- What the floor is: the amp check's per-chirp `floor` is the peak of a raw, unfiltered envelope (moving max of
  |x| over 6 samples) in the 75 ms before the chirp (`src/selftest/AudioSelfTest.cpp`). Any click or knock in
  that window raises it. Single high floors are room sound or a tick of the board, not a sign of the detector's band.
  The afternoon's 193k (owner at the bench, speaker on a carton) fits that; not attributed.
- No false detections from the node's own transmissions in 5 min of continuous traffic.
- Small and not a problem: with the radio on, the strip at the cap cost about 2.5 dB of SNR (set means 37.3 / 36.4
  dB against 39.5 / 39.2 dark; floor medians 2261 / 2739 against 1883 / 1986). With the radio off there was no
  difference (38.4 cap, 38.2 dark; floors 2020 / 2120). The sets ran at different times, so an
  interaction of light and radio is not shown; far from the 30 dB line. Noted, not tuned.

## State after the run

R1: `PARAM RESET` (store cleared), restarted, radio on (answers NET PING), strip off, call on (defaults).
E1-E3 still muted until they restart.
