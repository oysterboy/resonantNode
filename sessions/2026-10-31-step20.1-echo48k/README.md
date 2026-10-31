# 2026-10-31: step 20.1, the echo shell at 48 kHz on a Node board, and the OTA round trip

E5 (D-AMP Node, COM11) from the Node image 40ec12e7 to the echo shell f8dfe8ce (esp32dev-echo: 48 kHz, 8 DMA buffers
of 128 frames = 21 ms, a 500 ms int16 ring, the radio up) by `NET E5 OTA url=.../echo.bin` from E1, and back by the
same path to node.bin. E1 / E4 silent (stored), the room quiet; remote. echo_round_trip.log (token hidden).

| phase | length | dropped DMA buffers | loop pass mean / max | per-block path max | notes |
|---|---|---|---|---|---|
| quiet | 600 s | 0 | 77 / 935 us | 23 us | measured rate 47,999,984 mHz; 25 dropped at boot, before the counters' reset |
| TONE 1000 Hz x 20 (5 s each) | 120 s | 0 | 77 / 1,018 us | 23 us | env_peak 727,296 against 44,565 quiet: the own amp heard |
| radio: NET E5 ECHO STATUS every 3 s, NET * NET PING every 12 s | 121 s | 0 | 78 / 486 us | 23 us | rx 150, tx 90 frames; 99 reply lines at E1 |

Memory: the largest free block 110,580 bytes before the ring (48,000 bytes), 73,716 after it; free heap 119,852.
Boot line: `ECHO boot node=E5 rate_hz=48000 dma_bufs=8 dma_len=128 ring_ms=500 ring_bytes=48000
largest_block_before_ring=110580 net_started=1`.

Round trip: echo confirmed by OTA (its radio up), back to the Node image confirmed, `PARAM load stored=50 applied=50
stale=0 invalid=0` as before (the shell neither reads nor writes the Node's store), base 1 as stored.

Reading: at 48 kHz the per-sample path with the radio and the console costs ~0.2 us a sample; the loop's worst pass
(1 ms) is a twentieth of the 21 ms DMA queue. Nothing in it argues for 24 kHz. Not yet measured: the whisper image
(the round trip's third leg), a clip copy beside the ring, the echo detector itself.
