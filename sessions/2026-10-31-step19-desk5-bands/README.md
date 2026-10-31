# 2026-10-31: step 19.5, five Nodes free with a band per node (950-4950, scatter 1)

All five boards as Nodes on 40ec12e7 (E2 / E3 by OTA from the Analyzer / Emitter images, back at the end). Stored
values as in 2026-10-31-step17-desk5; for the runs, live (nodegroup --setup): output.silent 0,
behavior.base_scatter_bins 1, behavior.band_per_node 1 (range 950-4950, 13 positions of a 5-bin window). Seeds from
the roster (E1 0, E2 1, E3 2, E4 3, E5 1); a node wanders before its first idle call. Desk layout of 2026-10-26;
remote. Base analysis: tools/bench/basechains.py <log> 950.

## 1. Quiet, 30 min (1_free_run_quiet_30min.log, 1_summary.txt)

- 234 trains; at most 19 per minute; 105 chains, max 13.3 s, all ended within 30 s.
- 0 answers without another node's call in the 5 s before.
- Long chains (6+ trains): 10 (shared band, 2026-10-31-step17-desk5: 24), 90 % settled on one window.
- Pairs within 2 bins at the STATUS rounds: 66 %. The field split in two: a low group on 950-2200 and nodes high
  on 3450-4950 (bases played 0: 78, 1: 58, 2: 44, then 11: 33). A group that hears itself never wanders (the wander
  comes only after silence), and a one-bin scatter does not bridge seven bins.

## 2. Speech and music, 30 min (2_free_run_speech_music_30min.log, 2_summary.txt, 2_playback.txt)

bench_audio.wav four times from the lab PC, 17:39:15-18:08:16.

- 280 trains; at most 14 per minute; 96 chains, max 9.5 s, all ended within 30 s.
- Long chains 20, 70 % settled; pairs within 2 bins 94 %: the whole field stayed low (bases 0-2, 950-2450 Hz).
- **1 chain started by the music**: E3 on its lowest window (950-1950) logged a valid verdict at 371.7 s (strength
  3253, 111 ms) with no call in the 23 s before, during the end of the male-speech section, and answered it. Against
  0 in every shared-band run (2026-10-31-step17-desk5, 60 min x 5 Nodes) and 0 in the 1 kHz Node check
  (2026-10-31-step19-1khz, 7 min x 3 Nodes).
- No dropped DMA buffers.

## Reading

The mode works on the boards: windows move, glide and wander as designed, every chain ends, the rate stays low.
Two things differ from native-fieldsim: the field can split into a low and a high group that do not meet (66 % in
reach quiet), and the lowest windows let male speech through about once in 2.5 node-hours. Both point at the low
end of the range (the speaker and the room are louder there, so the low group hears itself well and keeps
together).
