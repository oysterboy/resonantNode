# 2026-10-16 5b candidate hop shapes, 100 cm, combined build (tools/bench/hopcand.sh)

Same build, placement rule and cells as 2026-10-16-hopcand-70cm, plus a rerun of hop5 with `PARAM scalar_max_duration_ms=300`. Single-tone captures at this placement: 2026-10-16-corpus-100cm (3,200 Hz weak: 12,300-13,300 against 27,700-36,300 for the other three).

| Cell | expected | amp P75 | contrast P75 | failures / detector duration |
|---|---|---|---|---|
| chirp 3,200 (comb reference) | 3/10 | 1,581-1,650 (3 inspected) | 32,746-32,750 | 7 detector: duration_too_short, 57-72 ms seen (3,200 Hz partly in a null) |
| hop5 25x5 gap 10, default params | 0/10 | - | - | 10 detector: duration_too_long, 213-214 ms seen, coverage 194-196 (limit 200) |
| hop4 30x4 gap 10 bin-aligned | 10/10 | 3,363-3,728 | 32,667-32,701 | 124-188 ms |
| hop5 25x5 gap 10, max_duration 300 | 10/10 | 3,148-3,289 | 31,862-32,043 | 205-211 ms |

The hop5 0/10 is a parameter, not the room: the 165 ms train plus a ~45 ms tail at this placement exceeds `scalar_max_duration_ms` 200 in every trial (at 70 cm the same train is seen as 182-184 ms, at 90 cm 152-154). With the limit at 300 every trial passes with a wide contrast margin. The 15 ms-gap train of the gate battery already needed 300; a 5-step train needs the limit raised in the profile (DET-011).
