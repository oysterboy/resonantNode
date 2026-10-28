# E004 - Where in 500-5000 Hz does the network signal work, and what does noise do there?

## Question

Owner 2026-10-27: "How bad is detection / noise acceptance really." The network band moved across tones of
500-5000 Hz; per position, does the hop train get through, and does speech, music or a plain tone get accepted?

## Status

phase 1 logged (2026-10-28): the 19 h bench sweep and a host preview. Open: hits under noise (a train arriving while
the detector is busy with music or speech), one lower Emitter level at the low end, and the band decision (step 11h).
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

## Reading

Noise acceptance is not the problem in this room with this material: nothing passed anywhere. The margin differs:
at 500-1250 the shape rule alone stands between music or speech and an answer; at 2750-5000 the noise does not even
reach the detector. Detection has one zone to avoid, 1500-3000 here. The shipped band sits in the zone where both
are best. Limits: one room, one distance, one noise file at one level, no birds (2-8 kHz, tonal glides: the
untested case for an outdoor site).
