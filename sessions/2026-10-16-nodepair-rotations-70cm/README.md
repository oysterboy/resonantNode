# 2026-10-16 late: E2's hop order rotated against E3 as listener. Inconclusive: E3 deaf, PARAM SET not applied

Intent: find out whether E3 (Node B) fails to hear E2's hop train because of E2's bin order (E2 rotation 4,
order 3950,2950,3200,3450,3700). Plan: E2 on the Node build, `PARAM SET output.hop_rotation <r>` for r = 3,0,1,2,4,
five kick chirps per run (`SELFTEST chirp n=5`), score E3's verdicts 0.15-0.7 s after each of E2's hop trains.

What happened:

1. `PARAM SET output.hop_rotation r` did not change E2's order: every run logs `order=3950,2950,3200,3450,3700`
   (rotation 4), and no `PARAM SET` reply was captured. E2 also rebooted at each port open (its `now=` restarts at
   ~5,700 ms at t = 5 s), so a set value would have been lost anyway. The test needs the Node not to reboot on the
   port open, or the rotation given before the run with the reply checked.
2. E3 heard **none** of E2's 25 kick chirps (5 runs x 5, plain 3,200 Hz, 100 ms, the shape E3 answered at 17:30),
   none of E2's hop answers, and later none of E2's Analyzer SELFTEST tones; `RB summary` on E3 after a fresh boot:
   pending=0 in 2 min. E2 heard E3's idle chirps every time (7 of 9 emissions across the runs). So since about
   18:00 E3 is deaf as a listener while E2, same firmware, hears.
3. E3's own SELFTEST: mic PASS with floor_mean 11,038 / floor_peak 184,981 in the first run (E2: 700 / 2,046), then
   538-625 / 1,439-2,315 in two repeats, own tone 179,000 at SNR 39-44 dB. A transient on the mic in the first run,
   normal afterwards; the deafness persisted across it.
4. E3's clock: in `rot3_45s` B logs `now=20718528` (5.75 h) 13 s into the run, 40 min after the board was flashed and
   booted; in other runs its `now` is sane. E3's USB reset is unreliable (sometimes the port open resets it, sometimes
   not), so some runs start on a fresh boot and some do not.

Reading: E3 as a listener is not trustworthy tonight, for a reason not yet found (mic supply/connection, I2S sample
clock, or firmware state after a reset that did not happen); E2 with the same build is fine. The direction E2 -> E3
must be measured with the Analyzer on E3 (SEQ DIAG shows i2s backsteps, clock corrections, dropped buffers) before
any conclusion about the hop order. The Node build should get a `RB STATUS` field for audio-source health
(sample-clock corrections, backsteps) so this is visible without the Analyzer.
