# 2026-10-30: step 17.1, the 8-bin band (2950-4700), host replay reference

Host only (no board). The T2h replay (every committed corpus folder, as in 2026-10-22-step8-baseline) re-recorded on
3e5ca34, the band default widened from 2950-3950 to 2950-4700 (decisions/2026-10-29-wider-band-base-bin-per-node.md).

t2h_diff_vs_8c8cdf5.txt against 2026-10-29-step14-maxdur450/t2h_replay.8c8cdf5.txt: 11 lines change, each the
replay's header line (`band=2950-3950` -> `band=2950-4700`); every occurrence and verdict line is the same. The
fixtures were recorded with 5-bin trains (2950-3950); the three new bins above them add nothing these captures
reach. t2h_replay.3e5ca34.txt is the reference from here on.
