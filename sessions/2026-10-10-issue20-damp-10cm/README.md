# 2026-10-10 issue #20: D-AMP at 10 cm (R2 ladder, first rung)

Setup: D-AMP Emitter E3 (COM4, 24:dc:c3:49:b7:38) -> D-AMP Analyzer E2
(COM3, 24:dc:c3:4b:4c:e8), 10 cm, speaker faces mic, same height. Firmware
965737a (board=damp) on both, tone 0.3 FS, 3200 Hz, 5 ms ramp. `SEQ DIAG off`,
`mode=detail` (the #26 baseline settings, ANA-004). Compare with piezo
bench:sessions/2026-10-08-issue7-10cm (baseline fbbc2db: T3 50/50).

| Run | Result |
|---|---|
| T3_scalar (TonalPulseScalar, 50) | 50 expected; detector 50/50, pattern-valid 50/50, 0 rejected; avg dt 21 ms, avg strength 22,480 |
| OBS_silent (50 windows, no emission) | 50 miss, 0 detector accepts: no false positives |

Dropped DMA buffers: 0 in all 100 windows.

## Amp check (AC, damp-bench-check.md R0/AC)

Own-mic sketch in `amp-check/sketch/` (the step 2 latency sketch, looped,
PASS at >= 30 dB; built against main's HAL at 0436644, unchanged at
965737a). 20 trials per board, logs in `amp-check/`.

| Board | Start | End |
|---|---|---|
| E2 | 20/20 pass, 47.8-51.7 dB | 20/20 pass, 43.5-50.1 dB (mean 48.4 vs 50.2) |
| E3 | 20/20 pass, 51.6-61.3 dB | 20/20 pass, 56.2-60.6 dB |

Both within 10 dB of the start check: the runs stand.

## Setup notes

- E3's USB auto-reset does not enter download mode ("Wrong boot mode
  detected (0x13)"); flash it with BOOT held + EN tapped, then esptool
  `--before no_reset`.
- A hand flash of E3 with `--flash_mode qio` in the bootloader header
  boot-looped (`load:0xffffffff,len:-1`). PlatformIO writes `dio` there for
  `board_build.flash_mode = qio` (the app still runs QIO); re-flashing with
  `--flash_mode dio` fixed it. Operator error, not the board, and not the
  hot re-wire one-off in decisions/2026-10-09-flash-qio80-and-i2s-sample-clock.md.
- The Emitter starts in AUTO; both runs send `EMIT MODE REMOTE` first
  (ANA-005). Without it a 3-trial check came out all `late` (dt 0.9-2.1 s).
