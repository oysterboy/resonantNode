# E004 - Where in 500-5000 Hz does the network signal work, and what does noise do there?

## Question

Owner 2026-10-27: "How bad is detection / noise acceptance really." The network band moved across tones of
500-5000 Hz; per position, does the hop train get through, and does speech, music or a plain tone get accepted?

## Status

phase 1 logged (2026-10-28): the 19 h bench sweep, a host preview, and hits under noise. Decided 2026-10-28: the band
stays (decisions/2026-10-28-band-stays-after-the-sweep.md); closed with step 11h.
Design: docs/research/frequency-tuning-sweep.md.

## Setup

- Bench: bench/sessions/2026-10-27-step11h-sweep24 (42f1eef on all five boards; E2 Analyzer / E3 Emitter at 79 cm;
  E1 / E4 / E5 Nodes silent, listening, their band set per position). 16 positions (band low edge 500-4000 in
  250 Hz steps, 5 bins of 250 Hz, plus the shipped 2950-3950), 8-9 rounds in a shuffled order, each visit: Analyzer
  hop10, a plain 150 ms chirp on the second bin, one RAW hop capture, bench_audio.wav played in full (435 s: drum
  and bass, female speech, male speech, flute) while the Nodes listen. Checks before it:
  bench/sessions/2026-10-27-step11h-checks.
- Host preview (2026-10-27): tools/bench/bandsweep.py host, the same file resampled, no room and no mic, gains
  16 / 64 / 256; numbers in the research note.

## Log

2026-10-28, the bench sweep (19.1 h, accepted):

- Noise: 0 accepted at every position over 8-9 full plays (about an hour of music and speech per position). Below
  1250 Hz the noise reaches the detector: 1,317 detections at 500-1500, 887 at 750-1750, 183 at 1000-2000 (all
  three Nodes, all plays; mostly the flute, then the male voice at 500); every one rejected, by the shape rule. From
  1250 Hz up the noise does not trigger the detector at all.
- Plain chirps: 0 / 1,330 accepted, rejected by shape everywhere.
- Hop train: accepted every time at 500-1250 (strength at the Strength16 cap at 500 and 750) and at 2250-5000. A
  weak zone at 1500-3000 (strength 4,400-7,100): 1500-2500 70/80, 1750-2750 80/90, 2000-3000 65/90, the losses
  moving between rounds. Read as the room (it comes and goes, like the 3200 Hz null of the shipped band), not the
  speaker.
- The shipped band 2950-3950: 90 / 90, strength 9,600-14,600, no noise detections.
- The host preview agrees on the shape of it: its accepts (dry file at made-up gains) fell at 1000-3000, none above
  5500.

2026-10-28, hits under noise (bench/sessions/2026-10-28-step11h-undernoise): hop10 back to back while the file
plays. 500-1500: 46 / 120 (flute 2/20, male speech 10/30); 750-1750: 76 / 120 (flute 6/20); 2950-3950: 120 / 120,
strength as in quiet. The silent Nodes agree (valid verdicts during the noise at 500: 36 / 105 / 116 of 120; at the
shipped band 120 each). Losses at the low bands: duration_too_long (the noise lengthens the occurrence),
failed_contrast, occurrence_seen_but_rejected.

2026-10-29, re-read per round and per bin (docs/research/room-comb-and-margin.md; tools/bench/sweepbins.py over the
RAW captures): 1500-2500 and 1750-2750 lost only in round 1 (0/10 each, then 70/70 and 80/80); between round 1 and
round 2 the per-bin level pattern shifted wholesale (2750 Hz a 15-35 dB notch in round 1, the strongest bin after;
the shipped band's strength 9,600 then 13,000-14,600), so round 1 was another room geometry. From round 2 on the
levels form a comb with dips at 1000, 1750, 2500, 3250, 4750 Hz (10-17 dB) that drift monotonically through the
night (2250 Hz from 48 to 97 dB). 2000-3000 keeps losing because two of its hops sit in the 2250/2500 dip at the
sweep's lowest strength; the neighbours carry dipped hops too and pass. The broad 3500-4500 hump is stable across
rounds and positions (the transducers, not the room).

## Reading

Noise acceptance is not the problem in this room with this material: nothing passed anywhere. Noise masking is: at
500-1750 music and speech in the band cost 40-60 % of the trains; the shipped band loses none. The margin differs:
at 500-1250 the shape rule alone stands between music or speech and an answer; at 2750-5000 the noise does not even
reach the detector. Detection has one zone to avoid, 1500-3000 here. The shipped band sits in the zone where both
are best. Limits: one room, one distance, one noise file at one level, no birds (2-8 kHz, tonal glides: the
untested case for an outdoor site).

2026-10-29: "one zone to avoid, 1500-3000" is withdrawn by the re-read above: the zone is a round-1 event plus one
position at its margin on this table, and the dips will sit elsewhere at the site. What the sweep supports is the
margin the shipped band carries over its gates and the hardware hump at 3500-4500 Hz
(docs/research/room-comb-and-margin.md).
