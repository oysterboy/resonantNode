# The Node ships the settings of the 2026-11-05 overnight field

Status: decided (owner 2026-11-09, "new defaults"), built the same day
Date: 2026-11-09

## Decision

`shippedNodeBehavior()` in `src/behavior/BehaviorProfile.h` is the Node's `behavior.*` default set: one window for
every node (`band_per_node` 1 on `band_range_low_hz` 2500 to `band_range_high_hz` 3500), `chain_length` 200,
`start_chance_pct` 85, `answer_spread_ms` 1000, `wait_after_heard_ms` 0, `after_own_no_answer_ms` 200,
`call_after_silence_ms` 30000 with a spread of 10000. `Node::configureParameters()` takes them before the registry
captures its defaults, so `PARAM RESET` returns to them. The struct's own defaults are unchanged: the host tests, the
field simulation and the echo modes (which set the fields they use) keep them. `node.radio_log` stays 0: it is a bench
aid, not a field knob.

## Why

- These are the values the five Nodes ran for two days without a reset, chosen one knob at a time on 2026-11-05
  (bench/sessions/2026-11-05-radio-log-lively, chains median 25 emits at start chance 85) and read in
  bench/sessions/2026-11-05-night-e0 (no reboot, stable heap, no dropped buffers from the 23:20 flash on).
- A board that has lost its saved values, or a new one, should play the field the owner chose, not the earlier guess.

## Rules out

- Retuning detection or the signal with this change: only `behavior.*` moved. The glide (75 ms) was already the default.

## Revisit when

- The room's November measurement (step 20.4) or a listening test in the tryout room says a value is wrong.

## Consequences

- A board that saved these values holds entries that equal the new defaults; `PARAM SAVE` once removes them
  (2026-11-09-save-only-what-differs.md). Boards that saved other values keep those until `PARAM RESET`.
- The overnight logs before 2026-11-06 name the old paths (mapping in 2026-11-06-param-review-owner-answers.md).
- `fieldsim` still starts from the struct defaults; its flags set the field explicitly.

## Source

Owner answer 2026-11-09 to "overnight values as code defaults?" (open since 2026-11-06).
