# 2026-10-26/27: step 11c, 24 h unattended soak with the shape rule and an hourly room check

Started 2026-10-26 11:38 Berlin, ends ~11:38 on 10-27. Nobody at the bench; no playback.
Firmware a33f293 on all five (DET-015 shape requirement). Layout: E2 Analyzer / E3 Emitter at one height, 79 cm;
E1 / E4 / E5 Nodes in the trial layout. `tools/bench/soakcycle.py --nodes E1=COM8,E4=COM10,E5=COM11 --analyzer
COM3 --hours 24 --cycle-min 60 --first-check-min 2 --status-every 120`.

- Network: E1, E4, E5 free (output.silent 0, unsaved; idle calls and answers), RB log full, RB STATUS every 120 s:
  soak.log, summarised by tools/bench/nodegroup_interim.py.
- Room check every hour (owner: "every hour, to catch room drift"): the Nodes silenced, the Analyzer runs hop10,
  chirp 3200 Hz / 100 ms and chirp 3450 Hz / 150 ms (10 trials each), and 2 RAW hop captures; then the Nodes run
  again. cycles.csv (one row per step per check), cycleNN_*.log, cycleNN_hop5_r*.csv (per-bin levels:
  `HOPBINS_GLOB="cycleNN_hop5_r*.csv" python tools/bench/hopbins.py <this folder>`).

Questions: does the shape rule hold over a day of idle calls and chains (every chain ends, 0 noise starts, no deaf
node); how much and when the room changes (chirp 3200 acceptance, per-bin levels, hop strength), against the
three unexplained shifts of 10-23 / 10-24 / 10-26.

First check (11:40): hop10 10/10, chirp 3200 / 100 ms 10/10 rejected duration_too_short, chirp 3450 / 150 ms 10/10
rejected by shape. Results when the run ends.

## Stopped after 1.63 h (owner: merge PR #66, fix its findings, restart the 24 h run)

Two room checks. 11:40: hop 10/10, chirp 3200 / 100 ms rejected duration_too_short, chirp 3450 / 150 ms rejected by
shape. 12:40: hop 10/10, chirp 3200 / 100 ms now rejected by shape (read long enough again: the room changed within
the hour), chirp 3450 by shape. Network (nodegroup_interim.py): 100 / 100 chains ended (max 4.9 s), 0 noise-started
of 1,109 answers, 0 dropped buffers after boot, 0 resets, max 28 per minute. The restarted run is
2026-10-26-soak24-shape-b.
