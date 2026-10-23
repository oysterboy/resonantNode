# The Node's network signal is the hop train; a plain chirp need not be heard

Status: decided; how to enforce it is open (see Measured): not by min duration, by shape (DET-015).
Date: 2026-10-23.

## Decision

A Node must hear the network signal, the 5-step hop train (25 ms x 5, gap 10, 165 ms, read 184-214 ms with the
room tail). It does not need to accept a plain single chirp (100 ms, read 116-154 ms). Owner's pick, asked in the
step 10 follow-up discussion.

## Why

- Every node answers with the hop train, and the idle call ends in it (decisions/2026-10-17-idle-call-wakes-the-
  network.md). No node emits a plain chirp in the field.
- It frees the min duration: today's 85 ms sits 0-2 ms from a 40 ms beep plus room reflection (read 86 ms,
  bench/sessions/2026-10-22-7d-dur40-raw), and the only thing that kept such beeps out was an early-inspection bug
  (audit 3.1). A min between the chirp's and the train's readings keeps short tonal sounds out by design.

## Measured 2026-10-23 (bench/sessions/2026-10-*, TonalPulseScalar SEQ, expected trials)

| shape | readings (ms) | note |
|---|---|---|
| hop train as shipped (25 x 5, gap 10) | **102-223** | the short ones (102-142: hopcand-80cm, bandcheck-90cm, hopshape-60cm, 7a-gate) are trains with a step in a room null; 5h's host notch sweep: one deep null still passes at 95 ms |
| plain chirp 100 ms | **91-198** | placement and room tail |
| 40 ms beep + room reflection | 85-87 | 2026-10-22-7d-dur40-raw |

The ranges overlap, so **the min duration cannot make the Node hop-train-only**. A min of 130 ms would still pass
most chirps, and it would reject real trains that lost a step to a null. What length can do is keep short beeps out:
a min near 100 ms sits above the beep plus reflection and at the shortest train seen (95-102), and still passes the
chirp, harmlessly. Hop-only needs a shape test: the train visits five band bins in order and a chirp or beep stays
on one. That is DET-015's distinct-bins / pulse-structure inspector (step 8a).

## Applied 2026-10-23

DET-017 set the min duration to 92 ms (owner), the middle of the measured window (beep 86, shortest null-hit
train 97). Bench: 40 ms beep rejected, 60/80 ms beeps and the chirp heard (bench/sessions/2026-10-23-det017-min92).
Hop-only itself waits for DET-015.

## Rules out

Using the plain chirp as a Node-to-Node signal. The Analyzer's 100 ms chirp T3 cells stay as a detector check,
with the Analyzer's own profile or a PARAM override, not as a Node requirement.

## Revisit when

A second signal shape is meant to be heard (8a's variation): the min duration then follows the shortest shape
that must pass.

## Source

Owner, 2026-10-23; DET-017 in docs/roadmaps/roadmap-detection.md.
