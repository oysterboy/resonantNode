# 2026-10-22: step 8 Part A gate (evaluator folded into inspection, FieldState trimmed)

Setup: as 2026-10-22-step8-baseline. E2 (COM3), E3 (COM4) Emitter 23d034a, 110 cm, trial layout; E1/E4/E5 held
silent by another session's open ports. Firmware main 4efba86 on E2 (Unity tests, then Node, then Analyzer).

| check | result |
|---|---|
| T4 on-device | test_shipped_profile 6/6, test_analyzer_pass_rules 9/9, test_timebase 8/8 (`*.4efba86.log`) |
| T7 Node smoke | 10 hop trains from E3 (`EMIT PULSES ...` over USB): detection 10 valid, Behavior received 9 and emitted 9. The first train came while the Node was still in ListenForQuiet (outputs off by design, issue #31; the train also spoiled the startup rebase). t7_node_smoke.4efba86.log. The first attempt sent the commands without the `EMIT ` prefix the Emitter needs over USB, so nothing played; that log was overwritten. |
| T3 chirp50 vs baseline | 50/50 expected, 0 drops; seqcmp: same counts and line counts; the only field difference is `integrity.evaluator_report_present` (removed on purpose); strength 5,800 vs 5,425, durations 116-118 / 125-126 ms (room drift through the day) |
| T3 hop10 vs baseline | 10/10 expected, 0 drops; same as above; 195-196 ms |
| T5 sizes (Analyzer boot) | DetectionRuntime 56,104 -> 47,008 B; InspectedOccurrence 912 and OccurrenceVerdict 84 unchanged; AnalyzerReport 1,156 -> 1,152 |

The host side (T1 builds, T2h byte-identical, T4 host suites, Node RAM -3,936 B) is in the commit messages of
5c5f9ed and 4efba86. E2 ends on the Analyzer (4efba86).
