# TUNING RUN Process

Purpose: repeatable scalar tuning loop for analyzer runs.

## Current scope

- Focus profile: `TonalPulseScalar`
- Goal: zero misses first, then tighten duration / hysteresis without bringing back duplicates
- Trial batch size: `50`
- Working session target: about `30` minutes

## Device setup

- Flash the analyzer build before starting a tuning batch.
- Use `COM6` at `115200`.
- Keep `DTR` and `RTS` inactive.
- Ensure no other serial monitor is holding the port.

## Start command

Use the analyzer console command:

```text
SEQ start profile=TonalPulseScalar tries=50 mode=source when=miss verbose=1
```

## Log layout

Each tuning batch gets its own folder under:

```text
logs/seq-tests/<timestamp>-scalar-hysteresis-<vals>/
```

Recommended files:

- `session.log`
- `run_01.log` through `run_10.log`

## Naming

Include the active scalar tuning values in the folder name so the run is self-describing.

Example:

```text
20260612_172657-scalar-hysteresis-300-20000-5000-30
```

Suggested value order:

```text
maxDuration-onsetThreshold-releaseThreshold-releaseDebounce
```

## What to capture

Per run, capture:

- `SEQ_TRIAL`
- `SEQ_SOURCE`
- `SEQ_SOURCE_SPEC`
- `SEQ_SUMMARY`

For the current scalar tuning focus, `mode=source when=miss` is the preferred diagnostic shape.
It keeps the run centered on the selected source chain while filtering for miss-side cases.

## What to check

For each run, look at:

- `miss_trials`
- `duplicate_trials`
- `late_trials`
- `avg_dt_ms`
- `avg_strength`

For misses, inspect:

- `SEQ_SOURCE`
- `SEQ_SOURCE_SPEC`
- `detail.scalar.inspect.reject_reason`

## Tuning rule of thumb

- First remove misses.
- Then tighten `maxTransientDurationMs` and hysteresis in small steps.
- Keep duplicates at `0` if possible.

## Current Python logger pattern

The current logging helper uses the PlatformIO Python runtime and writes one log per run.
The command sequence is:

1. Open the serial port.
2. Wait briefly for startup.
3. Send the sequence start command.
4. Read lines until `SEQ_SUMMARY`.
5. Repeat for 10 runs.

## Notes

- Do not depend on ad-hoc pasted serial text for tuning comparisons.
- Keep one folder per tuning variant.
- When a run is finished, summarise it before changing the next tuning value.
