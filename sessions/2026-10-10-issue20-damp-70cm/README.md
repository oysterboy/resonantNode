# 2026-10-10 issue #20: D-AMP at 70 cm (R2 ladder, third rung)

Setup: D-AMP Emitter E3 (COM4, 24:dc:c3:49:b7:38) -> D-AMP Analyzer E2
(COM3, 24:dc:c3:4b:4c:e8), 70 cm, speaker faces mic, same height. Tone
0.3 FS, 3200 Hz, 5 ms ramp. `SEQ DIAG off`, `mode=detail` (ANA-004).
Firmware: Emitter 965737a; Analyzer git=e5c65b0 (same source, docs-only
commit between). Compare with piezo bench:sessions/2026-10-08-issue7-70cm
(baseline 847b1ef: T3 48/50) and D-AMP at 0.1 FS
bench:sessions/2026-10-09-issue20-damp-70cm (46/50).

| Run | Result |
|---|---|
| T3_scalar (TonalPulseScalar, 50) | 50 expected; detector 50/50, pattern-valid 50/50, 0 rejected; avg dt 22 ms, avg strength 15,781 |
| OBS_silent (50 windows, no emission) | 50 miss, 0 detector accepts: no false positives |

Dropped DMA buffers: 0 in all 100 windows. Strength is higher than at 40 cm
(9,586): setup and orientation vary between rungs, as with the piezo runs.

## Amp check (AC)

| Board | Start | End |
|---|---|---|
| E2 | 20/20 pass, 42.6-51.7 dB (mean 49.7); tone 340k, floor 1,122 | 20/20 pass, 34.1-36.3 dB (mean 35.1); tone 333k, floor 5,937 |
| E3 | 20/20 pass, 56.4-60.6 dB; tone 913k | 20/20 pass, 57.5-60.3 dB; tone 870k |

E2's end SNR is 14.6 dB below its start, which the AC rule as written that
morning (drop > 10 dB invalidates) would count as a fail. The drop is E2's
quiet floor (1,122 -> 5,937), not its amp: its own-chirp tone stayed within
3% (297k-348k) in all six checks of the day, while its floor after each
session rose 1,143 -> 2,701 -> 5,937. The runs stand (owner, 2026-10-10):
every expected trial already proves E3's amp and E2's mic. The AC rule was
changed the same day to judge tone level, not SNR, and to run after a run
only when the run fails or degrades (damp-bench-check.md AC).

## E3 auto-reset (esptool)

E3's USB auto-reset resets the chip but never pulls GPIO 0 low ("Wrong boot
mode detected (0x13)"). Tried with esptool 4.11 custom reset sequences
(`esptool-reset-sequences/`, via ESPTOOL_CFGFILE, read_mac only): default,
GPIO 0 held 0.5 s and 1.0 s after EN release, two variants with DTR first:
none entered download mode. Not timing: the GPIO 0 side of E3's auto-reset
circuit is faulty. E3 needs BOOT + EN by hand for every flash; it stays the
Emitter so it is rarely re-flashed.
