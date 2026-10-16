# 2026-10-16 5b candidate hop shapes, 90 cm, combined build (tools/bench/hopcand.sh)

Same build, placement rule and cells as 2026-10-16-hopcand-70cm. Single-tone captures at this placement: 2026-10-16-corpus-90cm (all four tones strong, 22,600-43,000; no null in the band here).

| Cell | expected | amp P75 | contrast P75 | detector duration |
|---|---|---|---|---|
| chirp 3,200 (comb reference, 5a reference spacing) | 10/10 | 5,499-6,365 | 32,757-32,758 | - |
| hop5 25x5 gap 10 | 10/10 | 3,005-3,162 | 31,026-31,552 | 152-154 ms |
| hop4 30x4 gap 10 bin-aligned | 10/10 | 3,890-4,339 | 32,689-32,696 | - |

No failures. The 5a reference (plain chirp at 90 cm) is unchanged at 10/10.
