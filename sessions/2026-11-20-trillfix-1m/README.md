# 2026-11-20 trill fixes at 1 m, on the pitches the map found failing

R1 and E2 at 1 m, whole window (`--listen band --pitches`), windows 2550-3550 and 2600-3600, 60-s blocks, 1 round,
05:10-07:15. trill (11 x 20 ms) against two detection settings and two longer-pulse sounds:
trill-i16 / trill-i12 (the longest-stretch rule 16 / 12 ms instead of 20), trill25 (9 x 25 ms), trill30 (8 x 30 ms).
About 22 calls per cell.

| Sum over the 10 pitches | trill | trill-i16 | trill-i12 | trill25 | trill30 |
|---|---|---|---|---|---|
| R1->E2 | 152/225 | 227/231 | 217/221 | 220/220 | 228/228 |
| E2->R1 | 89/235 | 183/238 | 199/228 | 173/226 | 183/231 |

Every fix rescues the island failures (2600, 2850, 3300, 3350, 3600 ...). Left over, E2->R1 only:

- 3100: `strength_too_low` for every preset: E2 is too quiet at that pitch for R1; no pulse length helps.
- 2550: the plain trill passes (25/25), trill25 fails the island (2/23) and trill30 the amp check (0/21): a longer
  pulse at that pitch loses to what the room adds while the pulse is still on (a reflection arriving in time to
  cancel it), the comb in time.
- 3550: trill-i16 0/24, trill25 15/23; trill-i12 and trill30 pass.

Per-pitch rows: summary.csv.
