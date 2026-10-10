# 2026-10-10 issue #20: D-AMP at 40 cm (R2 ladder, second rung)

Setup: D-AMP Emitter E3 (COM4, 24:dc:c3:49:b7:38) -> D-AMP Analyzer E2
(COM3, 24:dc:c3:4b:4c:e8), 40 cm, speaker faces mic, same height. Tone
0.3 FS, 3200 Hz, 5 ms ramp. `SEQ DIAG off`, `mode=detail` (ANA-004).
Compare with piezo bench:sessions/2026-10-08-issue7-40cm (baseline 62a949e:
T3 10/50, 40 rejected amp_class=weak at strength ~4,000).

Firmware: Emitter 965737a; Analyzer reports git=e5c65b0 (PlatformIO rebuilt
it on upload after a docs-only commit; firmware source identical to
965737a).

| Run | Result |
|---|---|
| T3_scalar (TonalPulseScalar, 50) | 50 expected; detector 50/50, pattern-valid 50/50, 0 rejected; avg dt 22 ms, avg strength 9,586 |
| OBS_silent (50 windows, no emission) | 50 miss, 0 detector accepts: no false positives |

Dropped DMA buffers: 0 in all 100 windows.

## Amp check (AC)

Same sketch as bench:sessions/2026-10-10-issue20-damp-10cm/amp-check/sketch.
20 trials per board, logs in `amp-check/`.

| Board | Start | End |
|---|---|---|
| E2 | 20/20 pass, 47.7-52.3 dB (mean 51.0) | 20/20 pass, 37.7-46.2 dB (mean 42.4) |
| E3 | 20/20 pass, 37.1-59.8 dB (mean 55.9) | 20/20 pass, 51.6-60.8 dB (mean 58.0) |

Both within 10 dB of the start: the runs stand. E2's lower end values are
its floor, not its tone: own-chirp plateau median 348k at start, 347k at
end; pre-tone floor median 869 -> 2,701. E3's 37.1 dB start trial is one
pre-tone noise spike (floor 12,549; plateau 895k as in every trial).

## Setup notes

E3 needs BOOT + EN for every flash (auto-reset does not enter download
mode); flashed with esptool `--before no_reset --flash_mode dio`. See the
10 cm session README.
