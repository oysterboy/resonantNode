# 2026-10-14 Node build with the DC blocker (bfbe0c8): SELFTEST on E2, radio off

E2 Node build (default preprocessor: DC blocker 100 Hz, x0.588), 90 cm, facing up, E3 Emitter idle.
PASS: mic floor_mean 3,465-ish, quiet_boot smooth_at_rebase 32 / baseline 58.1, amp snr 37.0 dB,
own_chirp_suppressed PASS. The first run (2026-10-14-selftest-node-dc-e2) FAILED quiet_boot
(no_quiet, smooth_at_rebase 411) with the radio still playing; that run is noise, not the filter.

Smoothed signal magnitude over ~75 s in the quiet room (RB summary every ~0.8 s), radio off:
n=90, min 4, median 66, p90 254, one spike of 26,437 (an outside noise event). The quiet gate
for the Node (kRbStartupQuietThreshold 400, D-AMP) stays above the median and p90; First
Difference gave ~12-62 after a rebase, so the margin shrank from ~10x to ~2-6x.
