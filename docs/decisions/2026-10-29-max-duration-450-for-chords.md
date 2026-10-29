# Max duration 450 ms, so a chord of answers is not lost

Status: decided (owner 2026-10-29: "Maxduration", after step 14's blind window); implemented with this file.
Date: 2026-10-29.

## Decision

`detection.scalar_max_duration_ms` on the shipped profile goes from 300 to 450 ms. The two buffers that must hold
the longest occurrence grow with it: the feature history from 512 to 640 ms (`FEATURE_HISTORY_BINS`) and the
shape inspector's bin track from 512 to 1024 ms (`BinTrack::kMs`, a power of two for the 2^32 wrap).

## Why

Step 14 (lab E003 phase 2; bench/sessions/2026-10-29-step14-rotations, -chordcell): two Nodes answering 120-220 ms
apart sound to a listener as one sound longer than 300 ms, which the detector closed as too long. Both answers were
lost (host mix: every mix at equal level; board: E4 / E5 at +150 ms, 15 / 15 lost). At 450 the host mix counts them
as one valid train up to 220 ms apart and as two from 230 ms: no blind window. Any answer jitter or per-node offset
(BEH-006, BEH-008) can then use any spread.

The limit was the filter against long sounds; the shape rule (DET-015) now does that job:

- The recorded corpus (T2h set, 11 folders incl. noise and music) is unchanged in what it accepts. One music capture
  (radio_music_r4, 301 ms) now forms an occurrence instead of a too-long reject, and the shape rule rejects it
  (weak, medium required).
- Music and speech playback (bench_audio.wav, 2 plays, 875 s) with two Nodes at 450 and one at 300: no verdict on
  any of them (bench/sessions/2026-10-29-step14-maxdur450). Valid hop trains 10 / 10 on all three.
- A single hop train reads up to ~220 ms, well inside either limit.

## Cost

About 11 KB more static RAM on every board (Node 97 -> 108 KB, Analyzer 112 -> 124 KB of 327 KB). The Analyzer's
RAW capture needs one 96 KB block; checked after the change (see the step 14 entry).

## Rules out

Avoiding the window by timing alone (keeping every answer spread under 100 ms or over 230 ms).

## Revisit

If the tryout room (step 15) has reverberation that lengthens a single train toward 450, or if the shape rule is
loosened: the long-sound filter is then the duration limit again.
