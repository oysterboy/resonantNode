# 2026-10-11 SELFTEST negative controls (E2, E3 Node be61884)

Owner at the bench; runner `tools/bench/selftest.py` (652c9a6, other board held in reset during each test).

| Session | Fault made | Expected | Result |
|---|---|---|---|
| neg-mic | E2 mic SD (data, GPIO 33) unplugged | mic FAIL | E2 `mic FAIL reason=all_zero` (0 % non-zero), summary FAIL |
| neg-amp | E3 amp DIN (GPIO 32) unplugged | amp FAIL | E3 `amp FAIL reason=low_snr` -6.7 dB, tone -58.7 dB vs previous (DEGRADED), summary FAIL |
| neg-noisyboot | music playing through `--reset` | quiet_boot FAIL | both `quiet_boot FAIL reason=no_quiet` (smooth ~733 vs 400); E2 amp also FAIL (floor 30k, tone unchanged +0.1 dB) |

Gap found: with the mic data wire pulled, the floating GPIO 33 picks up the
I2S TX electrically, so E2's amp check read tone 891k, floor 0, 119 dB and
passed; quiet_boot and own_chirp_suppressed passed vacuously too (also
own_chirp with the amp DIN pulled). The summary is FAIL in every case, but
the dependent checks should report SKIP `mic_failed` / `amp_failed` and a
floor of 0 should be invalid. Firmware fix pending (docs/refactors/selftest.md).
SELFTEST needs a quiet room: noise fails the amp SNR while the tone stays
unchanged; the runner's tone-vs-previous column separates the two.
