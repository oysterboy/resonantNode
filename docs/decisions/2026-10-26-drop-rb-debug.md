# Drop `RB debug off|events` and the compile-time Node tracing behind it

Status: decided (owner 2026-10-26); implemented on the host 2026-10-27 (step 11g D); bench check with batch D's gate.
Date: 2026-10-26.

## Decision

The `RB debug` command, its manual line, `RB_VERBOSE_DEBUG` and the `NodeDebug::observe*` tracing it gates are
removed, with the stats nothing reads (`_coreLoopUs*`, `_fullLoopUs*`, `_debugChirpVisibleUntilMs`).

## Why

Review finding D2.5: `RB_VERBOSE_DEBUG` defaults to 0 and no env sets it, so the command switches nothing in any
shipped build while the manual documents it as event tracing; a compile-time knob behind a runtime switch, against
the rule that every knob is a registered param. `RB log full` and the bench tools cover what is traced today.
Owner answer 2026-10-26: "RB debug off drop".

## Rules out

Building the tracing in and keeping the command (the other option in D2.5).

## Revisit when

A field question needs per-event tracing that `RB log full` does not give; it then comes back as a registered
param, not a build flag.

## Source

Owner answer 2026-10-26 to the Part D sequence (D7); review D2.5.
