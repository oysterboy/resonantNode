# 2026-11-22 night: the safe fallback, trill-taper-rise or trill-taper-glide-riseclear

The line R1 - E2 - E4 - E3, 1 m apart, as the two nights before (../2026-11-20-line-map-trill-taper,
../2026-11-21-line-map-trill-taper). dd22ab0a, hearingtest.py at 9eccebba, presets at 35867f50. Each node plays in
turn while the other three listen, --listen band, --pitches, --block 45, one round, the same 15 pitches as 2026-11-21
(windows starting 2500, 2600, 2700). 19:00 - 02:03, no reboot, every command answered; nobody in the room.

The owner's question for the night: which call is the safe fallback. trill-taper, -glide and -glide-grain15 ran
beside them and stay open until they have run in the installation room (owner 2026-11-22). The owner chose
-glide-riseclear over -glide-rise (the glide kept off the rise) and -glide-grain15 over -glide-grain.

This file holds the comparison; the other folders' READMEs point here. R1 is the prototype (stripboard, its own mic
and case): the E pairs decide. E 1 m pools E2->E4, E4->E2, E3->E4, E4->E3; E 2 m pools E2->E3, E3->E2.

| Map | E 1 m | E2->E3 | E3->E2 | E 2 m | E4->R1 (2 m) | loud noise, false calls |
| --- | --- | --- | --- | --- | --- | --- |
| trill-taper, 11-21 | 96 % | 93 % | 100 % | 96 % | 61 % | 16 (11-20) |
| trill-taper | 95 % | 91 % | 96 % | 93 % | 64 % | |
| trill-taper-rise, 11-21 | 99 % | 93 % | 94 % | 93 % | 89 % | 0 |
| **trill-taper-rise** | 96 % | 99 % | 100 % | 99 % | 76 % | **0** |
| trill-taper-glide, 11-21 | 94 % | 80 % | 92 % | 86 % | 47 % | |
| trill-taper-glide | 97 % | 74 % | 93 % | 83 % | 50 % | |
| **trill-taper-glide-riseclear** | 92 % | 77 % | 89 % | 83 % | 39 % | **0** |
| trill-taper-glide-grain15, 11-21 | 91 % | 74 % | 87 % | 81 % | 42 % | |
| trill-taper-glide-grain15 | 92 % | 76 % | 92 % | 84 % | 50 % | |

Loud noise (../2026-11-22-line-falsealarm-loud): bench_audio.wav, nobody calling, 20 min each, window 2500-3500.
Both fallbacks 0 false calls on all four nodes; trill-taper-rise 0 for the second night.

What it says:

- Every map repeats its 2026-11-21 result within about 6 points (E pools): two nights agree.
- trill-taper-rise is the safe fallback: the best hearing at 2 m of every call on both nights (93 % and 99 %), 96-99 %
  at 1 m, the best R1 rows, and no false call in loud noise on either night.
- trill-taper-glide-riseclear keeps noise out as well, but hears like the glide calls (83 % at 2 m): the ending adds
  the noise rejection, the glide still costs its 10-15 points.
- The glide calls (-glide, -glide-riseclear, -glide-grain15) sit together at 81-86 % at 2 m.

Per-pitch rows: summary.csv in each map folder.
