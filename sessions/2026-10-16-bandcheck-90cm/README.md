# 2026-10-16 band 2950-3950 check, 90 cm (tools/bench/bandcheck.sh)

Same build, cells and rule as 2026-10-16-bandcheck-110cm; boards moved to 90 cm (the 5a reference spacing).

| Cell | expected | amp P75 | contrast P75 |
|---|---|---|---|
| chirp 3,200 Hz in band 2950-3950 | 10/10 | 5,356-5,711 | 32,757 |
| hop5 25x5 gap 10 on the new bins | 10/10 | 3,376-3,592 | 32,586-32,611 |
| chirp 3,200 Hz single target (reference) | 10/10 | 5,218-5,846 | 32,753-32,758 |

No failures; 5a reference unchanged.
