# 2026-10-21: speech/music OBS on the Node build (5b gate item, audit 2.2), phone radio at full volume, 50 cm from E2

Setup:
- All five boards on Node ecfc72b, free-running with idle off (`--setup "RB BEHAV idleEnabled=0"`) and the
  shipped detection and behavior values. `# state` lines are in the log header.
- `tools/bench/nodegroup.py`, 180 s.
- The owner's phone radio ran at full volume, 50 cm from E2. The program content (speech, music, or both) was
  not recorded. The radio was already playing when logging started.

Counted: a valid verdict with no other node's emission in the 0.05-0.8 s before it is a false trigger from the
radio. The run itself printed "unattributed verdicts: 4" with the old 0.15 s lower bound. Two of those four were
answers stamped 0.13-0.14 s after the sender's emit line; the bound is 0.05 s since this run.

| t | node | what |
|---|---|---|
| 0.54 s | E2 | valid; E1, E3 and E4 had invalid verdicts at 0.51-0.53 s, so a loud radio event was heard by all |
| 121.64 s | E1 | valid; no emission in the 20 s before it |

Result:
- **2 false triggers from the radio in 180 s** across five listening nodes (15 node-minutes). Each one started a
  chain over all five nodes. Emissions per minute were 12, 0 and 22. The rest rule ended each chain (4 rests,
  `rest_field_dense`).
- Compared with 5a (the Analyzer at 110 cm, speech 0/50 and music 0/50 windows, 2026-10-16), this is **worse**.
  The conditions also differ: full volume at 50 cm. The gate item ("no worse than 5a") is not met under these
  conditions, and a run at the 5a level and distance is needed to compare like with like.

## Run 2: OBS_speech_room_volume_1m

The owner's phone radio played **speech** at room volume, about 1 m from the nodes, for 180 s. Same setup as run 1
(idle off, shipped values); run 1's content was also speech (owner).

- Inside the logged 180 s: **0 false triggers**.
- The only activity was in the first 0.9 s: an E1-E2 chain, 6 emissions, that was already running when logging
  started (`fieldActive=1` at t = 0.02 s). It began during nodegroup's ~7 s setup window, before the log, so its
  origin (the radio, or something at setup) is unknown. The rest rule (`rest_field_dense`) ended it at 0.73 s.
  After that it was silent for 179 s.
- Reading: at room volume, 1 m away, speech gives 0 triggers in 180 s across five nodes. This is in line with 5a's
  speech 0/50. At full volume and 50 cm (run 1) it gave 2 in 180 s.
- Still open for 5b: music on the Node build. Per the owner this was the last phone-radio run: from now on, noise
  material is played from files through the lab PC's audio interface (Studio One box), so the level and content
  are reproducible.

## Run 3: OBS_music_flute_pc_box (file playback through the lab PC)

Material: `bench_audio.wav`, the 2026-10-16 noise file (`tools/bench/bench_audio.sections.txt`, normalised to
-24 dBFS RMS). Its sections music_dnb_tribal_behaviour (10-110 s) and tonal_flute_lp (325-425 s) were cut into one
200 s file. It was played with Windows SoundPlayer through the default device, the Studio One box at its last
volume setting (the owner's 2026-10-16 level). The speaker was fixed midway between the nodes along x and 40 cm
off the line along y. Playback started at 22:55:09, about 10 s into the 215 s log. The owner confirmed the music
was audible.

- Five nodes, idle off, shipped values: **0 valid and 0 invalid verdicts in 215 s.** No node even formed a
  candidate.
- `i2s.dropped_dma_buffers=0` in every STATUS line of all three runs.
- Reading: music (drum and bass) and a tonal flute at the 5a level give no false triggers on the Node build.
  Together with run 2 (speech, room level, 0), **5b's speech/music OBS item is met.** The caveat is run 1: speech
  at full volume 50 cm from a node gave 2 triggers in 180 s.
