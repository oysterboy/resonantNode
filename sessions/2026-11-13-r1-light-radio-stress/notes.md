# R1: radio stress with the light on (2026-11-13, 17:33-17:38)

Same setup as `../2026-11-13-r1-light-power/notes.md` (R1 USB + buck, E0 gateway, E1-E3 muted, speaker on the box,
call off). Script: `stress.py` (copy of the session's script); raw: `stress.json`, `console.log`.

Per strip state (`LIGHT 100` at the cap, then `LIGHT off`):
- A: 5 x SELFTEST over USB while E0 sends `NET R1 RB STATUS` back to back (49 requests, 205 reply frames from R1 in
  ~75 s): R1 transmits during the amp measurement.
- B: 5 x `NET R1 SELFTEST` from E0 (the reply streams while the test runs).

| strip | set | amp | snr_min dB | floors above 6k |
|---|---|---|---|---|
| white | USB under TX | 5/5 PASS | 20.7-36.0 | 16277 (run 5, chirp 5) |
| white | radio | 4/4 PASS | 28.0-36.9 | 6928 (run 1, chirp 3) |
| dark | USB under TX | 5/5 PASS | 35.8-37.1 | none (max 2828) |
| dark | radio | 4/4 PASS | 19.9-36.7 | 5809, 17645 (run 4, chirp 5) |

Spread 1.01-1.02 everywhere; mic PASS in all. RB STATUS deltas per strip state: i2s drops 0, overflows 0, read
errors 0, net.tx_failed 0, block_us_max +7 / +45 us, heap.free_min -1836 / -4 bytes.

The second radio test in each set has no result: the script sent it while R1 was still streaming the previous
reply (`SELFTEST busy`): a script artefact, not an R1 fault (it waited for the first SELFTEST_SUMMARY, which a
late part of the previous reply also carries).

Reading: single-chirp floor spikes of ~16-18k come with R1 transmitting, with the strip dark as well as lit; the
light does not add any. Smaller than the 193k of the afternoon set (speaker on the carton then). Nothing failed;
the check's median SNR held. Not a decision; the 30 dB line untouched.
