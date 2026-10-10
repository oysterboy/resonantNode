# 2026-10-10 issue #20 R1: Node checks on D-AMP E2

Node E2 (COM3, 24:dc:c3:4b:4c:e8), firmware 60b111f, env `esp32dev`
(board=damp), TonalPulseScalar, `RB log full` + `RB debug events`. E3 (COM4)
held in reset over USB (EN via RTS) so nothing else sounds; released for the
control. Script: `node_check.py`. Compare with the failing run of the day
before: bench:sessions/2026-10-09-issue20-damp-110cm-b,
node_quiet_boot_selfecho.4724fd6.log.

| Check | 2026-10-09 (4724fd6) | 2026-10-10 (60b111f) |
|---|---|---|
| Quiet boot | FAILED_NO_QUIET (smooth 233-254 vs threshold 20) | `RB rebase done` after 1.5 s (smooth 381 -> 284 -> 0, baseline 16.9; threshold 400 since b6e99b0) |
| Own idle chirps -> verdicts | every own chirp: `RB verdict=single_pulse ... decision=refractory_after_emit valid=1` ~0.9 s later | 12 idle chirps in 240 s, 0 verdict lines |
| External chirps (control) | - | 29 `RB verdict=single_pulse decision=consumed_pattern` in 30 s, each answered (`emit start source=heard_pattern`) |

Own-emit detection suppression (whole chirp + 60 ms tail, b6e99b0) keeps the
node's own chirps out of detection; the control shows the node still hears
and answers other chirps. Self-echo half of the #20 gate: passes on E2.
Not checked here: the same on E3 (needs BOOT + EN per flash).
