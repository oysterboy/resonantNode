# 2026-10-27: step 11h, checks before the 24 h sweep (42f1eef)

All five boards flashed with 42f1eef (main after 11g B1 / B2): E1 / E4 / E5 Node, E2 Analyzer (COM3), E3 Emitter.
Boot: the Nodes load 39 stored params (stale 0, invalid 0; output.silent 1), the plan prints module[2]
kind=BinShape, PARAM DUMP lists the eight detection.shape_* params.

tools/bench/bandsweep.py bench, one round each (positions.csv, noise.csv, pos*_r01_*.log, nodes.log):

| band | hop10 | chirp150 | strength (hop) | noise |
|---|---|---|---|---|
| 2950-3950 (shipped; 11g B2 smoke) | 10/10 | 0/10 | 9,837 | 20 s of the file: 0 valid, 0 rejected on all three |
| 500-1500 | 10/10 | 0/10 | 32,767 (cap) | - |
| 4000-5000 | 10/10 | 0/10 | 9,500 | - |

The Nodes take the band per position (PARAM SET ... status=ok) and get 2950 / 3950 back at the end. Playback
reaches them: E1's audio.level 10-170 in the file's leading silence, 400-3,800 (max 7,500-10,000) once the music
starts (RB STATUS every 3 s, 40 s). The shipped band's hop strength is lower than the soak's 14,500: the room
changed at 12:47 (soak24-shape-b's last checks).
