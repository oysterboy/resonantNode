# 2026-10-25/26: step 11a gate, five Nodes sounding with the radio on (12 h)

Setup: all five boards on Node 9ef51a1 code (remote commands over ESP-NOW, radio on), output.silent=1 stored and
switched off unsaved by nodegroup's setup (`PARAM SET output.silent 0`), so the boards boot silent again after the
run. `nodegroup.py --boards E1=COM8,E2=COM3,E3=COM4,E4=COM10,E5=COM11 --secs 43200 --status-every 120
--drift-bin 3600 --setup "PARAM SET output.silent 0"`, from 2026-10-25 ~17:55 Berlin. No radio traffic during the
run (the radio idles; the question is its cost to a sounding network). Nobody at the bench; no playback.

## Results (12.0 h; results.txt from tools/bench/nodegroup_interim.py)

| pass mark | radio on (this run) | block 2, radio off (2026-10-23-installation-run, 24 h) |
|---|---|---|
| chains ended within 30 s | 873 / 873 (max 6.2 s) | 1752 / 1752 (max 5.8 s) |
| noise-started answers | 0 of 15,858 | 0 of 30,546 |
| dropped buffers after boot | 0 (E2/E4/E5 at 18 from the boot, unchanged) | 0 |
| resets | 0 (uptime 11.97 h on all) | 0 |
| valid verdicts per hour per node | 311-418 | 303-418 |
| max emissions per minute | 53 | 49 |

Heap: heap.free and heap.free_min identical at 120 s and at 43,080 s on every node (87.2-87.7 KB): no leak. Loop
stack 12,060 B free. net.rx 0 (no radio traffic during the run, by design).

Log artifact: RB STATUS (~1.7 KB) reached the log in two pieces when the second UART burst came after
nodegroup.py's 5 ms readline timeout (E1 201 of 359 split, E2 67, E3 15, E4 2, E5 0). No data lost; the first
evaluation read the pieces as missing fields and false resets. nodegroup_interim.py now joins them; nodegroup.py
assembles whole lines for the next runs. summary_nodegroup.txt (its own end summary) has the old drift table.
