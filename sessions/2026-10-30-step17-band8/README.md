# 2026-10-30: step 17.1, the 8-bin band (2950-4700), host replay reference

Host only (no board). The T2h replay (every committed corpus folder, as in 2026-10-22-step8-baseline) re-recorded on
3e5ca34, the band default widened from 2950-3950 to 2950-4700 (decisions/2026-10-29-wider-band-base-bin-per-node.md).

t2h_diff_vs_8c8cdf5.txt against 2026-10-29-step14-maxdur450/t2h_replay.8c8cdf5.txt: 11 lines change, each the
replay's header line (`band=2950-3950` -> `band=2950-4700`); every occurrence and verdict line is the same. The
fixtures were recorded with 5-bin trains (2950-3950); the three new bins above them add nothing these captures
reach. t2h_replay.3e5ca34.txt is the reference from here on.

## desk/: the boards on the 8-bin band (2026-10-30 evening, remote)

All five by OTA to 1f0927a7, the Nodes again to 13902862 (the base re-seeded when the number of bases changes:
the Nodes had detection.band_high_hz 3950 stored, PAR-018, so the band booted at 5 bins with one base, and a live
PARAM SET to 4700 left the base clamped at 0 before the fix). Stored on E1 / E4 / E5: band_high_hz 4700,
output.hop_rotation 0 / 3 / 1 (PARAM SAVE); after it base_bin 0 / 3 / 1.

| part | result |
|---|---|
| Analyzer hop10 per base, band=2950-4700 (hop10_base0..3.log) | 10 / 10 expected at each base 0..3; shape.first_hz 2950 / 3200 / 3450 / 3700 |
| Analyzer chirps (chirp3200_100, chirp4450_150) | 10 / 10 rejected each (pattern_rejected) |
| nodecheck (nodecheck/) | 26 / 27: SELFTEST on E4 quiet_boot FAIL (no quiet at E4's reboot inside the check, floor_peak 25523 then); E4 rebooted and SELFTEST again: every check PASS. Level on: 1 answer, 9 by the level; level off: 10 / 10 |
| Glide on E1 (glide_E1.log; level_max 0, outputs on, live) | E1 at base 0 answers trains played at base 3 on 1, 2, 3, 3; then base 1 -> 2, base 0 -> 1: one bin per answer toward the base heard |

Music: in the overnight soak (2026-10-30-step17-soak8), every cycle.
