# 2026-10-26: step 11f bench gate (review leftovers batch 1, 0cbf30d)

Layout as in 2026-10-26-step11e-gate: E2 Analyzer / E3 Emitter at one height, 79 cm; E1/E4/E5 Nodes silent.

- Analyzer hop10 on 0cbf30d: 10/10 expected, seqcmp_hop10.txt against the 11e post run (597c921) identical in
  every count; strength 14,834 vs 15,078, dt 48 vs 45 ms (run-to-run).
- chirp10 on 0cbf30d: 10/10 expected (reads 118-119 ms), where 597c921 had 10/10 rejected at 08:24 (31-46 ms).
  chirp10_old597c921 at 09:16, the same build as this morning on both boards: 10/10 expected (126-127 ms). So the
  room changed between 08:24 and 09:05, not the firmware.
- a SEQ with pulses=60 (outside the Emitter's bounds) is refused (the run waits for a summary that never comes; the
  log was not written because the run was stopped). The message is built from EmitterLimits.h with the same
  numbers as before; not read back on the board.
- netcheck/netcheck.txt on 0cbf30d, five Nodes (E1 gateway): every step ok, 0 drops except E3's SAVE (9).
Left: all five as Nodes on 0cbf30d, output.silent=1 stored.
