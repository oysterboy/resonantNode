# The Node's signal is a 75 ms glide on 2000-6500 Hz; the detector's minimum follows it

Status: decided (owner 2026-11-05 at the desk: "5 gefällt am besten", "Range 2000-6500", "Ja" to shipping it)
Date: 2026-11-05

## Decision

- The network signal (output.hop_*): 5 steps of 15 ms with no gap, a 75 ms glide up the band bins, in place of
  5 x 25 ms with 10 ms gaps (165 ms).
- The range the per-node windows move on (behavior.band_range_low_hz / high_hz): 2000-6500 Hz, in place of 950-4950.
- The scalar detector's minimum duration 70 ms (was 92) and minimum coverage above release 60 ms (was 80).
- HopGapContract reads a train with gap 0 as one island (the whole glide), not one pulse.

## Why

- By ear (bench/sessions/2026-11-05-node-listen): the signal alone on one board at every window and in eight short
  shapes; the owner kept the short ones, liked gliding, picked 5 x 15 ms gap 0 over 8 x 25 and 3 x 15 gap 5.
  Windows below 1000 Hz and the top two (above 6500) were out; the range is the owner's call from both sweeps.
- Detection (bench/sessions/2026-11-05-shape-detect, E2 playing, E3 listening on one window): at 92 / 80 the
  glide passed 10 / 15, at 70 / 80 9 / 15, at 70 / 60 15 / 15 (read ~82 ms). The coverage was the limit.
- The 92 ms minimum (decisions/2026-10-23-node-hears-the-hop-train-only.md) kept short beeps out by length before
  the shape module existed; since DET-015 a sound without a climb is rejected by shape. bench_audio.wav (drum and
  bass, speech, flute) against two silent boards: no valid trigger at 92 / 80 or at 70 / 60; 70 / 60 let 10
  occurrences reach the inspectors against 3, all rejected as single_pulse. The host replay corpus: no accepted
  count changed; three captures gained a rejected candidate (two steady tones, one radio music).

## Costs and what it rules out

- A minimum-duration occurrence is released 20 ms before its 0..100 ms contrast and amp windows end (plan_check
  soft, earlyWindowMs 20); the runtime waits for the window (DET-017), the board passed 15 / 15. A shorter window
  would be its own tuning pass.
- A short beep with a climb now passes by length; tested only against one noise file and one window, not clicks,
  typing or a room. Revisit if the field shows false starts.
- The 25 ms x 5 train stays playable by PARAM SET (output.hop_*), and passes the new minimum.
