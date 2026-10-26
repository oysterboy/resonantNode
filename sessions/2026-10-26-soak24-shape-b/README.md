# 2026-10-26 to 10-27: step 11c, 24 h soak with room checks off the quarter hours (5fcd3fc)

Restart of 2026-10-26-soak24-shape (stopped after 1.6 h for the review Part D fixes: BinTrack across the ms wrap,
seq seed, host suites). All five boards on 5fcd3fc. E1 / E4 / E5 Nodes (output.silent 1 stored, set 0 unsaved by
the run), E2 Analyzer, E3 Emitter on its link at 79 cm, one height.

  python tools/bench/soakcycle.py --nodes E1=COM8,E4=COM10,E5=COM11 --analyzer COM3 --hours 24 --at-min 7,27,47 \
      --out bench/sessions/2026-10-26-soak24-shape-b --name soak

Room checks at minutes 7, 27, 47 of each hour (owner 2026-10-26: the hourly steps in the earlier drift are
suspicious; distant church bells ring on the hour and the quarters): three times as dense as the first run and
never during a chime. Each check: Nodes silenced, SEQ hop10 / chirp3200_100 / chirp3450_150 (10 trials each), two
RAW hop captures; cycles.csv one row per step.

Results: pending (the run ends 10-27 around 13:50).
