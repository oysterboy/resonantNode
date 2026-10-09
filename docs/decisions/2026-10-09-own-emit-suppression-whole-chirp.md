# Own-emit detection suppression covers the whole chirp plus a tail

Status: decided, implemented (b6e99b0); bench verification pending (#20 R1).
Date: 2026-10-09.

## Decision

A Node ignores its own mic for the whole time its chirp plays (any
pattern) and for `detectionSuppressTailMsOwnEmit` = 60 ms after it
(`ResonantBehavior::ownEmitDetectionSuppressed()`).

## Why

The old window was fixed from chirp start (100 ms, or 500 ms for idle),
so it ended inside longer patterns: the idle pattern's second pulse was
detected as a valid occurrence every time and only refractory kept the
node from answering itself; it still counted as field activity. The echo
lasts 33-54 ms after toneOff on D-AMP (TX queue + room).
Numbers: `docs/refactors/damp-bench-check.md` section 5,
bench:sessions/2026-10-09-issue19-damp-bringup (latency).

## Rules out

Hearing another node while this node is chirping (it is deaf then).

## Revisit when

Rooms make the echo outlast 60 ms, or step 6 shows nodes missing each
other because they chirp at the same time.

## Source

Owner, 2026-10-09, recommendation 6 in damp-bench-check.md section 6.
