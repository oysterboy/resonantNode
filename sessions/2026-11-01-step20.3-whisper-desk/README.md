# 2026-11-01: step 20.3, Whispernet on the desk (E5, then E1 / E4 / E5)

Boards: E5 on esp32dev-whisper d51a5391 (09:16, by NET E5 OTA from E1), then E1 and E4 on 545acdb7 (09:46, OTA on
their own ports). Source: the lab PC plays tools/bench/synthwhisper.py files (24 kHz) through its default device (the
Studio One box, its usual volume) with SoundPlayer; logs with the scratchpad scripts wdesk.py (one board) and
wchain.py (several). Boot: largest free block 110,580 before the line, whisper_bytes=100,800, delays E5 1717 ms,
E4 1611 ms, E1 1160 ms.

| run | values | result |
|---|---|---|
| wdesk1 | code defaults (onset 1500, loud 20000), files at -45 / -25 dBFS | whisper never reached the onset; speech passed as whisper (level P90 1,100-4,400, far under the bound): 4 answers |
| wdesk2 | `WHISPER GATE` with four keys: only loud=900 took (the parser bug fixed in 545acdb7) | speech, music: no pass; whisper still under the onset |
| wdesk3 | onset 250, release 170, floor 150, loud 900, -45 dBFS set | male whisper 6 passes (P90 160-280, mod 0.36-0.49), female 0; speech 0; fan 4 passes, music 2, claps 3 (P90 150-290, near the room's 40-100) |
| wdesk4 | same values, the set 10 dB louder (-35 / -20) | female 10, male 9 passes (P90 165-575, mod 0.31-0.58), Behavior answering, resting, busy; speech 0, music 0, fan 1 (mod 0.30, after 3.2 s) |
| wchain1 | E1 + E4 (E5 still on d51a5391 at onset 1500: deaf), gain 1 | after the file the two kept whispering each other's replays to the end of the 60 s watch |
| wchain2 | E1 + E4, gain 0.5 | the chain ended with the file; three single passes 20-45 s later (room sounds, not a chain) |

The stream: no underrun, no FIFO drop in any run (fifo_wait 640-1024 samples, 27-43 ms); E5 dropped 9 DMA buffers
over the session (pass_us_max 31 ms, block_us_max 1.8 ms). Heap with the line allocated: free 61,584, largest block
16,372: an OTA from Whispernet stalled after the WiFi join (E5, twice; a USB reset brought it back). Fixed in
748efe15 (the line and FIFO freed before the update), which the boards running d51a5391 / 545acdb7 cannot receive by
OTA: they need one USB upload.

Readings: on this mic and speaker a whisper sits 2-5x above the quiet room; at -45 dBFS the modulation test cannot
tell a fan from a whisper (both 0.3-0.45 near the floor), at -35 it can (fan 1 pass at the threshold). The loud bound
at 900 keeps speech and music out at this distance. Defaults set from these runs (545acdb7, 748efe15): onset 250,
release 170, floor 150, loud 900, gain 0.5. Not yet: a real whisper, distances, a RAW capture of whisper and replay,
the howl test with the loop closed at gain 1 on purpose.
