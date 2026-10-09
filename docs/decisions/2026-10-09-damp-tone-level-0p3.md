# D-AMP tone level 0.3 of full scale

Status: decided, implemented (b6e99b0); bench verification pending (#20 R1).
Date: 2026-10-09.

## Decision

The D-AMP chirp plays at 0.3 FS (`I2S_TONE_AMPLITUDE`, default in
`src/app/RuntimeDefaults.h`), up from 0.1.

## Why

At 110 cm the stock TonalPulseScalar profile goes from 0/50 to 50/50
between 0.1 and 0.3 FS; what limits it is the received level (amp
evidence), not the detector or the room floor. The amp is linear at 0.3.
Numbers: `docs/refactors/damp-bench-check.md` section 5,
bench:sessions/2026-10-09-issue20-damp-110cm-b.

## Rules out

Reaching range by lowering detection thresholds while the tone stays at
0.1 (see the open row on the amp inspector in README.md).

## Revisit when

Any node's amp distorts or drops out at 0.3 (one amp went silent after a
move the same night, cause unknown), self-echo or neighbour load in a real
room is too high, or the field trial needs more range.

## Source

Owner, 2026-10-09, recommendation 1 in damp-bench-check.md section 6.
