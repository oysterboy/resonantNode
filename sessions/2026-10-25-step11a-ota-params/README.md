# 2026-10-25: step 11a, remote params over ESP-NOW on five Nodes

All five on Node 267b860 code (the boot line reads build=f503a0c: built before the commit), silent (stored), no
playback, nobody at the bench. E1 (COM8) is the gateway; E2..E5 answer by radio. Scripts: nettest.py (function),
dropprobe.py (i2s.dropped_dma_buffers before and after each step).

Function (nettest.out): PING and PARAM GET answered by all four; NET E3 PARAM SET + SAVE, E3 rebooted, boots with
the value; SAVE restored; RB CALL refused at the gateway.

Cost (dropprobe.out, final build):

| step | dropped buffers |
|---|---|
| RB STATUS, idle 120 s with the radio on, serial PARAM DUMP on all | 0 on every node |
| radio PING, GET, SET | 0 |
| radio DUMP | refused (USB only) |
| PARAM SAVE, serial (E2) or radio (all) | 8-9 per node: the NVS write |

Earlier builds of the same session: a radio DUMP (17 frames per node) cost the gateway 52, 50 and 6 buffers as its
printing was throttled, 0 with printing off; hence DUMP / LIST are USB-only. Heap 94 KB free, heap.free_min
>= 93.7 KB, loop stack >= 13.4 KB free. Not done: a long run with the radio on and the nodes sounding.
Left: all five on 267b860 code, output.silent=1 stored, behavior.rest_ms back to 5000 on E3.
