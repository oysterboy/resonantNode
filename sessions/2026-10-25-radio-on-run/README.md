# 2026-10-25/26: step 11a gate, five Nodes sounding with the radio on (12 h)

Setup: all five boards on Node 9ef51a1 code (remote commands over ESP-NOW, radio on), output.silent=1 stored and
switched off unsaved by nodegroup's setup (`PARAM SET output.silent 0`), so the boards boot silent again after the
run. `nodegroup.py --boards E1=COM8,E2=COM3,E3=COM4,E4=COM10,E5=COM11 --secs 43200 --status-every 120
--drift-bin 3600 --setup "PARAM SET output.silent 0"`, from 2026-10-25 ~17:55 Berlin. No radio traffic during the
run (the radio idles; the question is its cost to a sounding network). Nobody at the bench; no playback.

In progress. At 3 h: i2s.dropped_dma_buffers unchanged since boot on every node (E1 0, E2 18, E3 0, E4 18, E5 18;
the 18 are from the boot when the ports opened). Results and the log when the run ends.
