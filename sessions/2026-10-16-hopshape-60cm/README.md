# 2026-10-16 5b: hop shapes that should survive one step in a comb null, 60 cm, combined build

Same placement and build as 2026-10-16-gate-combined-60cm (hop3 30x3 gap 10 was 6/10 there: one missing step leaves 58 ms, below min_duration 85 ms). Two shapes whose train minus one step still clears 85 ms.

| Cell | expected | detector view (trial 1) | amp P75 | contrast P75 |
|---|---|---|---|---|
| hop4 30x4 gap 10, hop=2800,3133,3467,3800 | 0/10 | dur 219, cov 194 | 3,207-3,394 | 28,330-29,241 (weak) |
| hop5 25x5 gap 10, hop=2800,3050,3300,3550,3800 | 10/10 | dur 113, cov 104 | 2,935-3,147 | 30,989-31,529 |

hop4 fails on contrast (7 of 10 inspected, all weak) and duration_too_long (3): its frequencies 3,133 and 3,467 Hz sit between the 250 Hz band-bank bins (2800, 3050, 3300, 3550, 3800), so the Goertzel peak bin is off-centre for two of four steps and the contrast collapses. Hop frequencies must be bin-aligned; a design rule for the Node hop (BEH-005 / OUT-005), not a detector fault. hop5 25x5 gap 10 passes at 60 cm with the smallest margin so far (30,989 vs limit 30,500); untested at the other spacings.
