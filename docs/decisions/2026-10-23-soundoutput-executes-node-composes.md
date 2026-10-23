# SoundOutput executes an OutputRequest; the Node composes it from Behavior's decision

Status: decided, implemented 2026-10-23 (step 10f, OUT-002/003).
Date: 2026-10-23.

## Decision

- `SoundOutput` (src/output/SoundOutput.h) executes one `OutputRequest` at a time: a hop train, a ChirpOutput
  pattern, or the idle call (announcement, gap, call). It owns the sequencing, checks the request against the
  device's limits (`OutputLimits`: 100-7900 Hz, pulse <= 1 s, gap <= 2 s, span <= 6 s) and keeps an `OutputStatus`
  (phase, started / done / rejected, last reject, last start / end, span), printed as `output.*` in RB STATUS.
- Behavior still decides what and when (`shouldStartChirp()`, `chirpPattern()`, `signalShape()`). The Node composes
  the concrete request from that decision and the `output.*` params, because the hop frequencies come from the
  detection band (FreqBandStream's grid) and the rotation, which Behavior does not know.
- A request that does not start (rejected or refused) falls back to the plain pattern, as the old code did. If
  nothing starts, the emission is ended at once, so Behavior does not wait in Chirping.

## Why

- The two-part idle call and the hop train lived in the Node glue with three loose flags. myspec 7.2 puts
  sequencing and the status in the output layer; Behavior and the Node now see one status.
- Composing in the Node keeps Behavior free of the band grid. Behavior building the request itself needs named
  variants (OUT-004 / NODE-005's ResonantProgram), which are deferred.
- Emission unchanged on the bench (bench/sessions/2026-10-23-step10f-output): same shape lines, same idle-call
  timing, E4 heard E1's trains at the same rate as before.

## Rules out

Output sequencing in the Node glue again; Behavior reading the band grid; a second path that drives ChirpOutput
for Behavior's emissions (the self-test's amp check still plays its tones directly, as an explicit hardware check).

## Revisit when

Behavior variants (OUT-004) or a second output device need Behavior to state the request itself.

## Source

Step 10f (owner change order 2026-10-23), issue #57; roadmap-output.md OUT-002/003.

Also fixed by the move: an idle call stopped during its gap (rebase, outputs off, self-test) used to start its
train afterwards; `SoundOutput::stop()` cancels the whole request.
