# 2026-10-22: step 8b gate (detection follow-ups), E2 / E3 at 110 cm

Setup: trial layout as 2026-10-22-step8-baseline. E3 (COM4) Emitter 23d034a; E1/E4/E5 held silent by another
session's open ports. E2 (COM3) ran main 8136d64 (Unity tests, first Node run) and ef86297 (Node re-measure, Analyzer
T3). E2 ends on the Analyzer (ef86297).

| check | result |
|---|---|
| On-device suites (8136d64) | test_shipped_profile 8/8 (incl. the continuous-tone and reject-count tests), test_analyzer_pass_rules 9/9, test_timebase 8/8 |
| Node smoke (8136d64), node_smoke_stack.8136d64.log | boot prints `RB plan_check status=warn history_needed_ms=410 history_ms=512 early_window_ms=5`; 10 hop trains from E3: 10 valid, 10 received, 10 emitted; `detection.verdict_queue_overflows=0`; `loop.stack_free_min_bytes=3640` |
| Node re-measure after moving getWindow's scratch off the stack (ef86297) | 10/10 received and emitted; `loop.stack_free_min_bytes=3416`, reached within 3 s of boot (before any detection) and unchanged by detections, so the boot / RB STATUS path, not detection, sets the minimum |
| T3 chirp50 vs step8-baseline | 50/50 expected, 0 drops; same counts and line counts; only `integrity.evaluator_report_present` differs (removed in step 8) |
| T3 hop10 vs step8-baseline | 10/10 expected, 0 drops; same as above |
