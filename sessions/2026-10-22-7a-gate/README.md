# 2026-10-22: step 7a gate (Analyzer diagnostics hygiene), E2 Analyzer / E3 Emitter at 110 cm

Setup: trial layout. E2 (COM3) ran the Analyzer and E3 (COM4) the Emitter, 110 cm apart. E1, E4 and E5 (Nodes)
were held silent. Commands: `tools/bench/seqrun.py`.

## Result: the gate is met (`final_*`, main 98b2fd0)

| run | trials | dropped DMA buffers | dt (anchor) | note |
|---|---|---|---|---|
| final_chirp_diag_detail | 20 expected | **0** | 51 ms in all 20 (command) | was 22-38 ms on the polled marker |
| final_hop5_diag_detail | 10 expected | **0** | 48 ms in all 10 (command) | 8/10 in the same setup while buffers dropped |
| final_dur40_diag_detail | 10 rejected | **0** | n/a (rejected) | 40 ms tones, correctly rejected (below the 85 ms minimum); `available_start_ms=na` 11 times |

Gate items (roadmap-0-steps.md 7a):
- detail + diagnostics with 0 drops on detected trials: met;
- dt on the sent command: met, and constant;
- RAW reports what it allocated: `RAW_ALLOC`, bench/sessions/2026-10-22-ana004-timing;
- invalid windows print na: met;
- the boot claim sent and acked: `EVT analyzer_control_claim sent / acked`.

## How it got there (the other runs in this folder)

1. `T3_chirp/hop5/dur40_diag_detail` (main 2a99951 + ANA-007): drops were back, 40-70 per trial, although the
   report took only 3.4 ms (ANA-004 fixed).
2. Bisect: bc9a89e gave 0 drops (`T3_bisect_bc9a89e`); 07ed3fe, the ANA-005 commit, gave 60 per trial
   (`T3_bisect_ana005`). The emitter state was not the cause: bc9a89e plus a manual early claim still gave 0
   (`T3_bc9a89e_manual_claim`). Drops appeared with SEQ DIAG off too (`T3_head_diag_off`).
3. A deeper DMA queue (8 x 128) did not help (`T3_dma8_*`): this was a sustained overload, not a spike.
4. Per-sample profile (`T3_profile2`, temporary instrumentation), in microseconds per sample during a trial:

   | part | in flash | in IRAM |
   |---|---|---|
   | AudioSignal + SEQ audio health | 27.7 | 12.0 |
   | FreqBandStream | 13.1 | 11.0 |
   | frequency packet | 6.4 | 2.1 |
   | DetectionRuntime::observeFrame | 17.8 | 9.6 |
   | events + SEQ bookkeeping | 8.0 | 2.3 |
   | sum | ~73 (budget 62.5) | ~37 |

   The hot path runs from flash, through many small functions, and the 32 KB cache misses. Whether a build stayed
   just under the budget or just over it depended on code layout, which is why an unrelated commit flipped it.
   With the per-sample path in IRAM (`IRAM_ATTR`, `T3_profile_iram2`): about 37 us per sample and 0 drops.
