# Params persist on PARAM SAVE only; all registered; a stored value wins over a changed default, with a warning

Status: decided, implemented 2026-10-23 (PAR-010/011; board check bench/sessions/2026-10-23-par010-persistence).
Date: 2026-10-23.

## Decision

Three owner picks for the Node's param persistence (NVS through Preferences):
- **Save model.** NVS is written only by `PARAM SAVE`. A `PARAM SET` stays live and is lost on reboot unless saved.
  `PARAM LOAD` re-reads the store into the fields and applies; `PARAM RESET` clears the store and restores the
  firmware defaults.
- **Scope.** Every registered param is saved, `output.hop_rotation` included. A saved rotation overrides the roster
  (app/NodeRoster.h) on that board until `PARAM RESET`.
- **Defaults.** At boot a stored value wins over the firmware default. Each entry also stores the default it was
  saved against. When that differs from the current firmware default, boot prints `PARAM stale path=...` with the
  stored value and both defaults, so a retune in firmware does not disappear silently under an old save.

## Why

- Save-only: a bench session tries values with PARAM SET and must not change the board for the next session by
  accident; a deliberate SAVE is the record. Flash is written only on request.
- All registered: one rule, no per-param flag to get wrong; the owner wants a board fully configurable in place.
- Stored wins: the board keeps what the operator saved. The warning keeps a firmware retune (e.g. DET-017's min
  duration) visible on a board that holds an older save.

## Rules out

Auto-save on PARAM SET; a per-param persistent flag; a firmware default silently overriding a saved value; a saved
value silently masking a new firmware default.

## Revisit when

A remote transport (PAR-013) needs to save without a serial operator, or the stale warnings become routine noise.

## Source

Owner answers 2026-10-23 (after "Persistence first okay?"); roadmap-param-config.md PAR-010/011.
