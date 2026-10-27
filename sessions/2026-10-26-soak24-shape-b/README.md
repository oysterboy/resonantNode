# 2026-10-26 to 10-27: step 11c, 24 h soak with room checks off the quarter hours (5fcd3fc)

Restart of 2026-10-26-soak24-shape (stopped after 1.6 h for the review Part D fixes: BinTrack across the ms wrap,
seq seed, host suites). All five boards on 5fcd3fc. E1 / E4 / E5 Nodes (output.silent 1 stored, set 0 unsaved by
the run), E2 Analyzer, E3 Emitter on its link at 79 cm, one height.

  python tools/bench/soakcycle.py --nodes E1=COM8,E4=COM10,E5=COM11 --analyzer COM3 --hours 24 --at-min 7,27,47 \
      --out bench/sessions/2026-10-26-soak24-shape-b --name soak

Layout (owner 2026-10-26): E5 -20 cm- E4 -30 cm- E1 in a row; E3 and E2 20 cm higher, 70 cm and another 70 cm
across (the E2 / E3 order may be the other way round). E4 stands in the direct line between E1 and E5: E5 <- E1 is
the weakest pair and the one that drifts (-13 % over the first 7.5 h, reverse direction steady), E5 <- E4 (20 cm)
sits at the Strength16 cap from about hour 3.

Room checks at minutes 7, 27, 47 of each hour (owner 2026-10-26: the hourly steps in the earlier drift are
suspicious; distant church bells ring on the hour and the quarters): three times as dense as the first run and
never during a chime. Each check: Nodes silenced, SEQ hop10 / chirp3200_100 / chirp3450_150 (10 trials each), two
RAW hop captures; cycles.csv one row per step.

Results (stopped at 23.45 h, 10-27 13:10; owner: "Soak is accepted at 23h+"):

- Room checks: 70 (13:47 to 13:07). Hop train 710 / 710 expected (one more hop10 than chirps: the last check was
  cut). Strength 14,480-15,120 for 23 h, then 11,480 at 12:47 and 10,240 at 13:07 (the room changed in the last
  hour, the 3200 chirp read stronger at the same time; still 10 / 10). Chirp 3450 / 150 ms: 0 / 700, failed_shape
  every time. Chirp 3200 / 100 ms: 0 / 700, duration_too_short in 57 checks (from 17:07 on; the room's 3200 null),
  failed_shape in 13.
- Network (nodegroup_interim.py): 16,136 emissions, 1,378 chains, all ended within 30 s (max 5.1 s); 0 answers
  started by noise of 14,671; 0 resets, 0 verdict-queue overflows, no dropped buffers after boot (E5's 18 in the
  first two minutes); max 32 emissions per minute.
- Drift: E5 <- E1 down 17 % by midnight, then flat (E4 between them, see Layout); other pairs within +2..+9 %;
  E5 <- E4 at the Strength16 cap from hour 3.
- Bells: no rejected detection near :00 / :15 / :30 / :45; valid detections at those minutes at the average rate.
  The every-2.00 s sound at 13:39-13:46 (heard by all three, rejected, gone when the first check took the Analyzer
  / Emitter) did not come back.
