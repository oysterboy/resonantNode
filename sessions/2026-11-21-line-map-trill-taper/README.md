# 2026-11-21 night: what in glide-grain costs the hearing, and trill-taper-rise

The line R1 - E2 - E4 - E3, 1 m apart, as the night before (../2026-11-20-line-map-trill-taper). dd22ab0a,
hearingtest.py at 9eccebba, presets at 35867f50 (the glide-light and grain-15 ones new that day). Each node plays in
turn while the other three listen, --listen band, --pitches, --block 45, one round. The same 15 pitches as the
2026-11-20 glide-grain map (windows starting 2500, 2600, 2700), so the maps compare pitch by pitch. 19:00 - 05:32, no
reboot, every command answered. Nobody in the room, nothing moved; the owner had not heard the new presets.

Why: the owner finds trill-taper-glide-grain the most attractive sound; on 2026-11-20 it lost 5-30 points against
trill-taper, the misses mostly "invalid, contrast too low".

This file holds the comparison; the other folders' READMEs point here. R1 is the prototype (stripboard, its own mic
and case): the E pairs are the comparison, R1 rows below for completeness.

Glide = each trill pulse falls 150 Hz, 4 ms soft edges, first 3 pulses swell; glide light = 75 Hz, 2 ms. Grain = a
broadband burst above 6500 Hz under each pulse, 30 % of the tone's peak; grain15 = 15 %.

Heard, the 15 pitches pooled (in brackets: pitches at 90 % or more, of 15). E 1 m pools E2->E4, E4->E2, E3->E4,
E4->E3; E 2 m pools E2->E3, E3->E2.

| Map | E 1 m | E2->E3 | E3->E2 | E 2 m | R1->E4 (2 m) | E4->R1 (2 m) |
| --- | --- | --- | --- | --- | --- | --- |
| trill-taper, 11-20 | 98 % | 81 % (10) | 99 % (15) | 90 % | 99 % | 65 % |
| trill-taper (control) | 96 % | 93 % (14) | 100 % (15) | 96 % | 99 % | 61 % |
| -glide | 94 % | 80 % (10) | 92 % (14) | 86 % | 90 % | 47 % |
| -glidelight | 99 % | 75 % (10) | 87 % (11) | 81 % | 97 % | 53 % |
| -grain | 92 % | 77 % (11) | 76 % (10) | 77 % | 89 % | 48 % |
| -grain15 | 97 % | 84 % (12) | 97 % (14) | 90 % | 97 % | 64 % |
| -glide-grain, 11-20 | 93 % | 65 % (8) | 73 % (9) | 69 % | 89 % | 14 % |
| -glide-grain15 | 91 % | 74 % (10) | 87 % (13) | 81 % | 96 % | 42 % |
| -glidelight-grain15 | 91 % | 71 % (10) | 89 % (13) | 80 % | 91 % | 40 % |
| -rise | 99 % | 93 % (14) | 94 % (14) | 93 % | 100 % | 89 % |
| -glide-grain-rise, 11-20 | 83 % | 73 % (8) | 89 % (12) | 81 % | 62 % | 32 % |

Loud noise (../2026-11-21-line-falsealarm-loud, bench_audio.wav, nobody calling, 20 min, window 2500-3500):
trill-taper-rise 0 false calls on all four nodes (11-20: trill-taper 16, -glide-grain-risefall 1).

What it says:

- The control repeats the night before within about 6 points at 2 m: the room held.
- grain at 15 % is nearly free (E 2 m 90 %, as trill-taper's 90-96 %); at 30 % it costs ~15 points.
- The glide costs at 2 m in either strength (86 % and 81 %); the light one keeps 1 m at 99 %.
- Together they cost the most: -glide-grain 69 %, with the grain halved 80-81 %; the light glide gains nothing over
  the full one there.
- trill-taper-rise hears like trill-taper (99 % / 93 %, and the best R1 rows of both nights: E4->R1 89 %) and let no
  false call through in the loud noise. The ending's shape check and 20 ms stretch rule keep noise out without the
  hearing cost the glide-grain endings carried.

Per-pitch rows: summary.csv in each map folder.
