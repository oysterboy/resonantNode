# 2026-10-14 bench checks for the code-review fixes (#32, main e4d6975)

E2 Analyzer (COM3) with E3 Emitter (COM4, UART link), 110 cm as left from
2026-10-13 (not re-measured). Builds of main e4d6975; all six envs compile.

| # | Check | Result |
|---|---|---|
| 1 | six envs build | pass |
| 2 | PARAM SET/STATUS echo (same serial session) | pass |
| 3 | T3 `dur=40` then normal | short: 6 expected / 4 rejected (duration_too_short x2, pattern_rejected x2); normal 10/10; no PipelineQueueOverflow, 0 misses, 0 buffer_overrun. No miss occurred, so NoOccurrence classification itself was not exercised |
| 4 | `SEQ start profile=Bogus` | pass: ERR, SEQ STATUS shows no run |
| 5 | Node refractory=0 survives `rebase force`; rebase mid-chirp: no self-answer; 300-char line -> `RB ERR line_too_long` | pass (run on E2 Node, not in this folder) |
| 6 | Freq T3, 60 trials, ms clock offset 2147303648000 us (2^31 ms ~3 min after boot) | 51 expected / 9 rejected, 0 miss, same reject rate before and after the crossing. The `TIMEBASE offset_us` line is not in the logs: offset taking effect is unverified |
| 7 | forced loop stall during a chirp | not provoked: heavy output (verbose=2 samples=on, RAW dump) gave buffer_overrun_trials=0; there is no stall hook. Open |

Note: opening the serial port resets the board, PARAM changes do not survive
between sessions.
