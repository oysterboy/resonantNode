# The idle call wakes the network: an out-of-band announcement, then the node's own call

Status: decided (owner), implemented a4a368c, bench pending.
Date: 2026-10-17.

## Decision

A node's idle call (after `idleTimeoutMs` of quiet) is an announcement tone outside the network band followed
by the node's own call as configured, today its hop train (the plain chirp when `output.hop_enabled` is 0): `output.idle_announce_hz` = 300 Hz for `output.idle_announce_ms`
= 500 ms, a 200 ms gap, then the hop train on the band bins. The others do not detect the announcement and do
hear the train, so an idle call wakes the network the way a heard call does. Behavior stays in Chirping through
the whole sequence and own-emit suppression covers it.

Reproducing the behavior before it (rule 2 of `2026-10-17-behavior-contract-and-reproducible-sessions.md`):
`PARAM SET output.idle_announce_hz 0` gives the old two-tone idle pattern (2,000 Hz then 3,200 Hz, 500 ms each,
200 ms apart).

## Why

The owner's answer to "should an idle call wake the network or only sound": it wakes. The old pattern woke it
too, but by accident: its 3,200 Hz half is on a band bin and 500 ms long, so the others heard a plain chirp, not
the network signal. Tying the wake-up to the node's call keeps one signal for the network and leaves the
announcement free as an aesthetic layer.

## Rules out

- An idle call the others cannot answer (a pure presence sound).
- The announcement inside the network band.

## Revisit when

The rest tuning shows the group never falls quiet because of idle calls (2026-10-17 tuning: idle was 39 % of all
emissions at the quietest setting), or OUT-006 (two-layer signal) gives the announcement its own design.
