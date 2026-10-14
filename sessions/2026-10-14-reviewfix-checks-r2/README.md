# 2026-10-14 bench checks for the tier R2 fixes (#32 checks 14-21, main 568dbc0)

E2 Analyzer (COM3) + E3 Emitter (COM4), 110 cm as in
2026-10-14-reviewfix-checks (not re-measured). Node checks ran on E2 as Node
(output only on the console, see below), with E3 as Node (e4d6975) for chirps.

| # | Check | Result |
|---|---|---|
| 14 | boot baseline | smoothedSignal 243 right after boot, 12-62 after the quiet rebase and after `RB rebase force` (several readings 12, 25, 62). The "D-AMP floor 230-250" is the startup offset; note and 400 gate need a tuning pass (not changed) |
| 15 | Freq duration | **not confirmed.** `dur=100`: durationMs 148-157 (not ~100). `dur=40`: 10/10 accepted at 94-95 ms, not rejected as duration_too_short (logs here) |
| 16 | stale Behavior | pass x2: E3 chirp, E2 `RB rebase` 80 ms later -> verdict `decision=outputs_off`, no answer after the rebase |
| 17 | parsing | pass: `RB PARAM freqScore=x` ERR unchanged; Analyzer `PARAM scalar_min_strength=abc` ERR; `RB BEHAV foo=1` ERR; `SEQ start perod=3000` ERR unknown token; `period=25OO` ERR bad value; `sampleLead=400` SAMPLES_WARN lead_beyond_history. **Fail: `RB rebase forced` starts a quiet-gated rebase.** Not checked: BEHAV echo paste, `PARAM SET ... 1.5 junk` on the Node |
| 18 | OBS default | no OBS_WARN with `SEQ OBS start tries=20`; the window value was not read back |
| 19 / 7 | forced stall | compile-time `-D TEST_LOOP_DELAY_MS=200` (the loop delay is a build flag, not a SEQ option; `SEQ start loopDelayMs=` is ERR): T3 10 trials, `buffer_overrun_trials=10`, dropped_dma_buffers rising ~350 per trial (1107 -> 3557). Analyzer side only; Node `RB summary` side and the 1-1.5 s stall length were not done |
| 20 | SELFTEST | pass: amp trial 1 `valid=1`, 5/5 valid; `RB rebase force` during SELFTEST -> `refused busy=selftest`. Idle-chirp part not done |
| 21 | Emitter | pass: `EMIT CHIRP freq=17000` -> ERR not playable; `EMIT SWEEP start=3000 stop=9000` -> ERR |

Node console output for 14/16/20 is not in this folder; selftest session
2026-10-14-selftest-e2-node-568dbc0 holds the SELFTEST log.
