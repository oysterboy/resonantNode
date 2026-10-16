# 2026-10-16 band 2950-3950 check, 70 cm (tools/bench/bandcheck.sh)

Same build, cells and rule as 2026-10-16-bandcheck-110cm; boards moved to 70 cm (the spacing where the plain
chirp was 0/10 in the morning gate and 10/10 in the afternoon).

| Cell | expected | amp P75 | contrast P75 |
|---|---|---|---|
| chirp 3,200 Hz in band 2950-3950 | 10/10 | 3,206-3,498 | 32,754-32,756 |
| hop5 25x5 gap 10 on the new bins | 10/10 | 3,190-3,392 | 32,402-32,513 |
| chirp 3,200 Hz single target (reference) | 10/10 | 3,139-3,458 | 32,753-32,755 |

No failures. Over the three spacings (110, 90, 70 cm): 90 of 90 trials expected.
