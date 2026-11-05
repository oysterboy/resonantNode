# 2026-11-05 night: what the radio costs the audio (radioload.py through E0)

Five boards in one mode at a time (Node 0f8ac9c9, echo, whisper, switched by NET OTA from E0), three phases of 60 s
each: quiet (node.radio_log 0), log (20, the most the mirror sends) and log + storm (`NET * PARAM DUMP` every 10 s: every
board answers ~160 lines in 8+ replies at once). Counters: each board's own dropped DMA buffers (RB STATUS
i2s.dropped_dma_buffers on the Node, ECHO STATUS dropped_bufs on echo and whisper), read before and after a phase.
node.txt / echo.txt / whisper.txt: the lab PC played claps_sparse.wav (echo) or a recorded whisper x8 (whisper) in a loop;
the Node's boards made their own traffic (idle calls and chains at the lively settings).

| mode | quiet | log 20 | log 20 + storm | log lines/s delivered (log / storm) |
|---|---|---|---|---|
| Node | 0 on all five | 0 | 0 | 3.3 / 7.6 |
| echo | 0 | 0 | 0 | 1.4 / 2.1 |
| whisper | 0 | 0 | 0 | 1.0 / 1.1 |

Also after ~100 min of the Node run at node.radio_log 20 (NET PING, 2026-11-05 night): queue_full 0, tx_failed 0,
outbox_full 0 on every board; log_dropped 33 of ~5,200 lines on each (0.6 %), when the cap met a burst.

Reading: nothing dropped in any mode, with the storm on. What this did not reach: the mirror's cap (20 lines/s a board =
100 frames/s into one gateway; each line ~100 bytes is ~10 KB/s, the gateway's UART at 115200 takes 11.5 KB/s). The boards
printed 0.2-0.7 lines/s each here; a denser field (more boards, a long chain at RB log full) is what would meet the cap.
So the answer for now: node.radio_log 20 is safe in all three modes at today's activity; the next limit to measure
is the gateway's UART (a higher baud there), not the boards' audio.
