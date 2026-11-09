# PARAM SAVE stores only what differs from the firmware default (PAR-018)

Status: decided (owner 2026-11-06, with the param review), built 2026-11-09
Date: 2026-11-09

## Decision

`ParamRegistry::save()` writes a param only when its value differs from the firmware default captured at boot, and
removes the stored entry of a param that equals its default (`ParamStore::erase`). The `PARAM SAVE` line reads
`written= unchanged= removed= failed=`; a default-valued param with no entry counts as unchanged. The stored entry keeps
its format (value and the default it was saved against); `PARAM load` and the stale line work as before.

## Why

- Saving every param pinned each one at today's default: step 14's `detection.scalar_max_duration_ms` 300 hid the new
  450, and a SAVE of 450 pinned eight more params (roadmap-param-config.md, PAR-018). With the shipped Node values
  now in code (2026-11-09-node-ships-the-overnight-field.md) a saved copy of them would hide the next change too.
- The owner's rule of 2026-11-06: the boards' stored params must not hide new defaults before the room.

## Rules out

- A per-entry "set by the user" flag: the value against its default says the same without a new store format.

## Consequences

- `PARAM LOAD` returns every param to its default first and then applies the store: the persisted state is the
  defaults with the entries over them. The bench scripts that undo their changes with `PARAM LOAD` (`nodecheck.py`,
  `soakcycle.py`, `maxdurnoise.py`) rely on it. At boot the fields hold the defaults already.
- A value set by hand to the default and saved is dropped, and follows later defaults: that is the point.
- A board saved before this change keeps its entries until the next `PARAM SAVE` removes the ones that equal the
  default. `output.silent 1` is not a default, so a saved silent board stays silent.
- Tests: `test_param_registry` (a default-valued param is not stored, a value back at its default is dropped),
  `test_param_commands`.

## Source

PAR-018 (roadmap-param-config.md), the owner's answer of 2026-11-06 in step 20.5.
