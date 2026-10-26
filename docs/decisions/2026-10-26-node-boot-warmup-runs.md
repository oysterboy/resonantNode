# The Node's boot baseline runs: mic warmup first, then the quiet gate

Status: decided (owner 2026-10-26); not yet implemented (step 11g, batch D).
Date: 2026-10-26.

## Decision

The Node boots through `RBBaselineState::Boot` again: the 2 s mic warmup (`_audioSignal.begin` with the rebase,
`kRbStartupMicWarmupMs`) runs first, then `ListenForQuiet` as today. The warmup was lost, not replaced on purpose.

## Why

Review finding D2.4 (docs/research/architecture-readability-review-2026-10-25.md): `Node::begin()` skips the warmup
(`begin(false)`) and sets `ListenForQuiet` directly, so the Boot state, its "RB warmup waiting" case and the
SELFTEST's `== Boot` check are dead, and the boot comment describes a sequence that does not happen. Owner answer
2026-10-26: "boot run".

## Rules out

Deleting the Boot state and the warmup constant as dead code.

## Revisit when

The bench shows the warmup changes nothing (the first minutes after boot read the same with and without it), or
VEKTOR's boot sequence replaces the Node's.

## Source

Owner answer 2026-10-26 to the Part D sequence (D7); review D2.4.
