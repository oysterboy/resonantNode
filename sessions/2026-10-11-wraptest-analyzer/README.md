# 2026-10-11/12 #30 wrap test: Analyzer across the 32-bit us wrap

Analyzer E2 (COM3) `esp32dev-analyzer-wraptest` at 189b24c
(`TIMEBASE_OFFSET_US=4114967296`: the us clock wraps 180 s after boot, at
time-base 4,294,967 ms). Emitter E3 (COM4) unchanged, 70 cm. Each run:
`EMIT MODE REMOTE; SEQ DIAG off; SEQ start profile=TonalPulseScalar tries=10
mode=detail when=all verbose=1`.

| Run | time base (ms, uptime_ms) | Result |
|---|---|---|
| T3_pre (seqrun, reset) | ~4.115 M | 10/10 |
| T3_post1, T3_post2 (no reset) | 4.367-4.41 M, after the wrap | 10/10, 10/10 |
| SELFTEST_post | after the wrap | mic PASS, amp PASS 54.2 dB |
| T3_span (after a new reset) | 4.253-4.282 M | 10/10 |
| T3_after_span | 4.291-4.320 M: wrap between trial 2 (4,294,554) and trial 3 (4,297,754) | 10/10, dt 24 ms every trial |

Before the fix, every run after the wrap read 0/10 (two soaks, #27).
Process notes: T3_post*/SELFTEST_post/T3_span/T3_after_span were run without
reset by a scratch script (benchlib.run_commands), because seqrun --no-reset
cannot take the firmware tag from a session; T3_span/T3_after_span came from
an earlier background job whose reset script had hung on a read without
timeout - it continued after that script was killed and ran 138 s after the
next reset, i.e. as planned. Logs are complete.
