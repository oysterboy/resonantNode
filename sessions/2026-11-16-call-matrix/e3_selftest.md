# E3 checks, 2026-11-16 19:20-19:30 (every board muted, matrix stopped)

`NET E3 SELFTEST` (image 6e56f53b), through E0:

| check | E3 | E2 (reference, same minutes) |
|---|---|---|
| board | PASS | PASS |
| mic floor_mean / floor_peak | 1547 / 28332, then 1429 / 26759 | 706 / 16274 |
| quiet_boot | FAIL no_quiet (boot at 17:41, smooth 1253 at the 20 s timeout, state FAILED_NO_QUIET), PASS after a later rebase | PASS |
| amp SNR | 38.5-40.0 dB, 5/5 valid, no reboot | 45.2-49.0 dB |

Quiet-gated `RB rebase` with every board muted: E3 20 s without 1 s of quiet (smooth mostly 75-370 under the 400
gate, bursts above it reset the count; `rebase skipped reason=no_quiet smooth=606`); E2 quiet in 3 s. E3 then passed
a rebase with node.radio_log 0 and again with 20 (done in 1.1 s): the radio log is not the cause; the gate passes or
fails on whether a burst comes.

Reading: E3's own floor is about twice E2's, with bursts, and sits near the startup quiet gate (400); the amp's SNR
is 8 dB under E2's for the same reason. Board-level (mic or supply), not the room: everything was muted. Its reboots
when it plays (2026-11-16-low-window-field) point at the supply too. Not checked: its supply, cable, mic wiring, by
the owner.
