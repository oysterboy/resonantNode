# 2026-10-16 5b candidate hop shapes, 110 cm, combined build (tools/bench/hopcand.sh)

Same build, placement rule and cells as 2026-10-16-hopcand-70cm, plus hop5 with `PARAM scalar_max_duration_ms=300` (100 cm needed it). Single-tone captures at this placement: 2026-10-16-corpus-110cm (3,300 Hz in a null: 11,000-12,300 against 19,000-37,800).

| Cell | expected | amp P75 | contrast P75 |
|---|---|---|---|
| chirp 3,200 (comb reference) | 10/10 | 2,596-2,884 | 32,749-32,754 |
| hop5 25x5 gap 10, default params | 10/10 | 2,943-3,118 | 32,277-32,378 |
| hop4 30x4 gap 10 bin-aligned | 10/10 | 3,308-3,429 | 32,714-32,725 |
| hop5 25x5 gap 10, max_duration 300 | 10/10 | 2,775-2,993 | 32,288-32,360 |

No failures; the train stays under 200 ms here, so the default limit holds at this placement.

## Afternoon summary, hop5 25x5 gap 10 over all six spacings (60 cm: 2026-10-16-hopshape-60cm)

| Spacing | 60 | 70 | 80 | 90 | 100 | 110 |
|---|---|---|---|---|---|---|
| hop5 25x5 gap 10 | 10 | 10 | 9 | 10 | 0 (10 with max_duration 300) | 10 |
| hop4 30x4 gap 10 bin-aligned | - | 10 | 6 | 10 | 10 | 10 |
| chirp 3,200 (same moment) | - | 10 | 10 | 10 | 3 | 10 |

First signal with >= 9/10 at every spacing in one afternoon, given `scalar_max_duration_ms` 300. Caveats as for the morning battery: one room, one day, 10 trials per cell; the comb moved between the morning and the afternoon (chirp at 70 cm 0/10 then 10/10).
