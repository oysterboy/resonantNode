# E002 - Does shape separate the hop train from other sounds? (DET-015 verification)

Status: phase 1 logged 2026-10-26 (host replay over the recorded corpus). Step 11b (#59): the owner asked to
verify that shape separates the signals before the data path for DET-015 is chosen.
This file holds observations only; the decision it feeds is DET-015's data path (docs/roadmaps/roadmap-detection.md).

## Question

Can a fact about the strongest band bin over time tell the Node's hop train (25 ms x 5 steps, 10 ms gaps, bins
2950 / 3200 / 3450 / 3700 / 3950 Hz in rising order from the node's rotation) apart from a plain chirp, a short
beep, an in-band sweep and speech / music, in the recordings we have?

## Method

`native-replay --shape` (src/host/Replay.cpp, host only): the recordings run through the real chain (AudioSignal,
FreqBandStream in band mode, DetectionRuntime, shipped TonalPulseScalar); per millisecond the strongest band bin
(FreqBandStream::lastPeakBinHz) while the target value is above the release threshold. For each verdict, over
[start, start + duration]: runs of one bin (runs under 4 ms folded away), distinct bins, steps one bin up (or the
wrap from the top back to the bottom), and the dip in the target value and in the broadband energy at each bin
change. A capture with no verdict gets the same over its whole active stretch. Corpus: test/fixtures,
bench/sessions/2026-10-16-corpus-* (60-110 cm, sweeps, notch / gap variants), 2026-10-22-corpus-freqcols-*,
2026-10-23-cell-desk-ref-110cm, 2026-10-23-cell-height25-110cm, 2026-10-16-corpus-noise, 2026-10-22-7d-dur40-raw.

## Phase 1 results (2026-10-26)

| signal | verdicts | distinct bins / steps up | reads as |
|---|---|---|---|
| plain chirp 3200 Hz, steady tones | 64 | 1 bin, 0 steps, every one | `3200x130` |
| hop train, 60-110 cm, two rooms | 19 | 5 bins; 5 steps up (17), 4 (2) | `2950x35,3200x37,3450x30,3700x56,3950x19,...` |
| hop, dry / wet gap variants | 3 | 4-5 bins, 3-5 steps up | `3200x11,3450x25,3700x55,3950x4` (dry: first step lost) |
| hop, E3 raised 25 cm (no verdict on the host; capture view) | 5 | 5 bins, 5 steps up | all five steps above the release level |
| sweep 2950 -> 3950 Hz, 100 / 150 ms | 5 | 4-5 bins, 4-7 steps up | `2950x13,3200x25,3450x23,3700x21,3950x8` |
| radio speech / music | 0 verdicts | - | stopped by the existing gates before any occurrence |
| 40 ms beep | 0 verdicts | - | stopped by the 92 ms minimum (recorded band=off, no bin track) |

Observations:
- Hop train vs any steady tone: clean. Every steady tone stays on one bin; every hop train visits 4-5 bins rising.
- Hop train vs an in-band sweep of the same rate: not separable here. Run lengths overlap (hop runs 30-37 ms, the
  room fills the 10 ms gaps; sweeps 21-34 ms per bin), and so do the dips at the bin changes, in the bin's level
  (hop 14-58 %, sweep 21-55 %) and in the broadband energy (hop 4-44 %, sweep 5-31 %).
- The height-offset trains, which the board read as ~108 ms and the host not at all, still show all five steps
  rising over the capture: the detector's envelope ended at step 3 because the last steps are weak, the bin track
  did not. A shape fact read over a window longer than the occurrence sees the whole train.
- Dip depths vary strongly with geometry (reference vs height cell), so a dip-based rule would be room-dependent.
- The trailing `3700,3950` runs after each train are the room's tail on the last two bins.

## Not covered yet

- Speech and music that pass the existing gates (none in this corpus): the case where shape matters most for noise.
- Other rotations (every recording starts at 2950; a Node with rotation 2 starts at 3450 and wraps).
- Two trains overlapping (two nodes answering at once).
