# 2026-10-26: step 11b, DET-015 shape requirement on the bench (a33f293)

Layout: E2 Analyzer / E3 Emitter at one height, 79 cm; E1/E4/E5 Nodes, silent (stored) unless a step says
otherwise. All five on a33f293 (track, inspector, the shipped shape requirement).

Analyzer SEQ, 10 trials each, mode=detail:

| run | played | result | reason |
|---|---|---|---|
| hop10 | hop train, rotation 0 | 10/10 expected | |
| hop10_rot2 | hop train starting at 3450 (wraps) | 10/10 expected | |
| hop10_random | 3700,2950,3450,3200,3950 | 10/10 rejected | failed_label=shape |
| sweep10 | sweep 2950 -> 3950 in 150 ms | 10/10 expected | accepted by decision |
| chirp10 | 3200 Hz, 100 ms | 10/10 rejected | duration_too_short (the room's 3200 null this morning, 31-46 ms; shape not reached) |
| chirp10_3450 | 3450 Hz, 150 ms | 10/10 rejected | failed_label=shape (read 210 ms) |
| chirp10_3700 | 3700 Hz, 150 ms | 10/10 rejected | failed_label=shape (read 248 ms) |
| chirp10_3200_150 | 3200 Hz, 150 ms | 10/10 rejected | failed_label=shape (read 100 ms) |

Nodes (nodegate11b.py / .out, selftest_ext.out):
- E1 (silent off, unsaved): 10 Emitter hop trains -> 10 valid verdicts, 10 answers; 10 chirps (3450 Hz, 150 ms) -> 10
  verdicts, 0 valid, 0 answers.
- E5 SELFTEST chirp n=5 plays five hop trains (its rotation 3950,2950,...); E4 SELFTEST external, started first and
  E5 triggered once E4 printed SELFTEST_EXTERNAL waiting: external_chirp PASS, latency 346 ms. (A first try that
  started E5 before E4 was waiting gave SKIP no_external_chirp: E4 heard all five, valid, before its wait began.)

Host: t2h_replay.a33f293.txt is the new T2h reference (the 27 steady-tone captures rejected by shape against
2026-10-22-step8-baseline's dump; nothing else changed).
Left: all five as Nodes on a33f293, output.silent=1 stored; E2/E3 as Analyzer / Emitter.
