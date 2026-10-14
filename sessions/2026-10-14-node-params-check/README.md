# 2026-10-14 #8: Node registered scalar params (code 89b39ec + the registration commit), E2 Node, E3 Emitter

Owner away; no room notes. The boards had been re-plugged earlier, so the placement is no longer the
90 cm of the battery: the Analyzer build (unchanged code) now sees 3,200 Hz as 24 ms events
(T3_scalar_10 here: 0/10, strength ~4,800), i.e. a null at the current placement. That made the Node
hear no chirps at the defaults, which is what the behavioral test uses:

Node, Emitter in AUTO (chirp every 2 s, 8 chirps per 16 s window), RB verdict lines counted:

| Node params | verdicts |
|---|---|
| defaults (scalar_min_duration_ms 85) | 0 |
| scalar_min_duration_ms 10, scalar_min_longest_island_ms 0, scalar_min_release_coverage_ms 0 | 14 |
| scalar_min_duration_ms back to 85 (island, coverage still 0) | 0 |

`PARAM LIST` shows the 13 `detection.scalar_*` params (floats, uint32 ms values, bools); out_of_range and
parse_error status checked (`scalar_max_gap_ms 99999`, `scalar_require_min_strength maybe`).
