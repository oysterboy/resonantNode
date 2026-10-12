# 2026-10-12 #30 wrap test: Node across the 32-bit us wrap

Node E2 (COM3) `esp32dev-wraptest` at 189b24c (us wrap 180 s after boot;
`TIMEBASE ... next_us_wrap_in_ms=179987`). Emitter E3 (COM4) reset into AUTO
(chirp every 2 s), 70 cm. `RB log full` + `RB debug events`, 370 s.

| Window | verdicts (all valid) | answers (emit source=heard_pattern) | idle emits |
|---|---|---|---|
| 0-180 s, before the wrap | 88 | 88 | 0 |
| 180-370 s, after the wrap | 95 | 95 | 0 |

One verdict per E3 chirp (~1 per 2 s) and one answer per verdict before and
after the wrap: detection, behavior and own-emit suppression (no extra
verdicts from E2's own answers) hold across the wrap.
