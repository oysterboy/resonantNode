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

## After the USB upload (10:09, 4561106b with the OTA fix, on E1 / E4 / E5; the owner's go)

E5 updated by OTA from Whispernet to Whispernet (100 %, confirmed on app1): the way back to the Node image is open.

| run | result |
|---|---|
| wchain3: three boards, defaults (gain 0.5), whisper_female / speech_male / whisper_male at -35 / -20 | female whisper weak (2 passes, E5 none); speech 2 quiet passages passed detection, Behavior declined both; male whisper answered by all three at once, the rest holding the follow-ups; after the file, clusters every 10-20 s with two or three boards at the same moment (room sounds, not a chain: a chain steps by the 1.1-1.7 s delays) |
| wquiet1: 90 s, nothing played | 27 passes, clusters every ~10-20 s, P90 150-330, mod 0.35-0.82, mostly on several boards at once; the room's level 30-90 |

Reading: at floor 150 (2-5x the quiet room) Whispernet answers the room's quiet, modulated sounds as whispers, about
every 10-20 s with three boards. A higher floor trades those for quiet whispers (the -35 dBFS whispers read
165-575 on E5): the owner's call, with a real whisper on the desk to set it.

## Recordings at the boards' mics (10:35-10:50, 28b427e3: WHISPER REC)

Source position: the lab PC and its speaker sit right next to E3 (the Emitter; owner 2026-11-01). wrec1: 8 min of
the quiet room, each board recording its first 4 passes (66 passes in all); wrec2: the -35 dBFS whispers played
(E1_rec5 came 0.5 s before the file started: a room pass, left out); wrec3: the -20 dBFS speech with the loud bound
at 5000 for the run. WAVs in */rec (24 kHz int16, 300 ms before the phrase to 1.2 s after the check), decoded with
recwav.py; features by recfeat.py (recfeat.txt):

| class | <300 Hz | 300-1k | 1-3k | 3-6k | 6-11k | flatness 300-6k | voicing | peaks | crest |
|---|---|---|---|---|---|---|---|---|---|
| room (12) | 0.30 | 0.14 | 0.14 | 0.15 | 0.23 | 0.86 (0.64-0.90) | 0.74 | 12 | 2.5 |
| whisper, synthetic via the speaker (7) | 0.03 | 0.64 | 0.10 | 0.05 | 0.01 | 0.17 (0.09-0.29) | 0.49 | 8 | 3.8 |
| speech (9) | 0.71 | 0.24 | 0.00 | 0.00 | 0.00 | 0.02 (0.01-0.07) | 0.78 | 7 | 4.3 |

The room's passes are broadband and nearly flat (white-noise-like, a quarter of the energy above 6 kHz) with ~7
small bumps a second and a low crest: rustling or typing, heard by all three boards at once. Spectral flatness
separates them from both the synthetic whisper and speech by a wide margin here. Caveat: the synthetic whisper
through this speaker has its energy at 300-1000 Hz; a real whisper carries its fricatives (s, sh) at 4-8 kHz and
reads flatter. Real whispers are needed before a threshold is chosen.

## The timbre check on the boards (11:06-11:20, f9b204b9; flat= 0.25)

| run | result |
|---|---|
| wflat1: 5 min quiet room, three boards | 4 passes in all (before: ~27 in 90 s); the room's phrases fail on the timbre at 0.33-0.51 or on the level; one pass at 0.25 (the bound) |
| wflat2: whisper_male, whisper_female (-35), speech_female (-20) | the whispers 23 and 26 passes over the three boards, speech 0; the timbre failed 2 phrases |

Load with the eight biquads per sample: block_us_max 1.6-1.9 ms against the 5.3 ms block, pass_us_mean 142 us, no
dropped DMA buffer over the 5 min.

## The owner at the desk (15:10-16:15)

- wsess1 (15:10, b352e8f9's predecessor f9b204b9 with flat=1, WHISPER REC 50; the owner whispered without the pauses):
  real whispers read flatter than the synthetic ones. Board flatness of the recordings: whisper 0.5 m 0.16-0.45, 1 m
  0.03-0.49, 2 m 0.17-0.36; speech at 1 m 0.04-0.41 (median ~0.07); rustling / typing 0.19-0.76 (median ~0.45). The
  owner's speech at 1 m (level 558) sat under his whisper at 0.5 m (863): the loud bound cannot tell them apart.
- wsess2 (15:22, b352e8f9, range flatmin 0.15 .. flat 0.40, gain 0.5): whisper 0.5 m 23 passing checks, 1 m 1, 2 m 18,
  speech 3, rustling 2. The owner heard no replay at all: gain 0.5 plays the mic's digital level, inaudible at a metre.
- wgain1-3 (E5 only answering; E1, E4 on=0): gain 16 still inaudible; gain 64 loud enough, but E5 answered room noise
  near the floor (long phrases at level 165-200) and its own replay (louder than the whisper at its mic: the loop grew
  until the loud bound cut it).
- wgain4 (onset 400, release 280, floor 300, gain 64): the room itself sat at 150-260 (one "phrase" of 78 s); the owner:
  "good loudness, hard cut at start and end, sometimes louder then cut off (own output), sporadic, and on outside
  transients".
- wsoft2 (16:09, f3ddba37: no listening during the own playback plus 300 ms, 200 ms pre-roll, 300 ms hold, 40 ms fades,
  gain 64, onset 400 / release 280 / floor 300): the owner: "works, but unresponsive for a while after a replay";
  reacted to rustling (chair casters on the floor, typing). 31 phrases put to Behavior in 5 min, 19 answered, 12 not:
  resting 8 / rest_level 6 counted with declined_by_level 2 (the echo room's level 4 and 4 s rest), on top of the deaf
  time during each own playback (~D plus the phrase).

Open after the day: rustling and chair noise still pass (the timbre range is not enough on real sounds); Behavior's
level and rest are the echo room's and too slow for a conversation; the threshold follows no room level (an adaptive
floor); a phrase longer than D is cut where the own playback starts. All three boards left on f3ddba37 with on=0
(listening, silent).
