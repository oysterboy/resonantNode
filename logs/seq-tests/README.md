# Scalar Tuning Run Set

Status: ready to launch

## Goal

- Reduce misses to zero
- Keep duplicates at zero
- Then tighten duration and hysteresis gradually
- Run a 100-trial sweep with small, controlled parameter shifts

## Current settings

- Profile: `TonalPulseScalar`
- Max duration: `300 ms`
- Onset threshold: `20000.0`
- Release threshold: `5000.0`
- Release debounce: `30 ms`
- Min duration: `60 ms`
- Min peak strength: `0.0`

## Analyzer command

```text
SEQ start profile=TonalPulseScalar tries=50 mode=source when=miss verbose=1
```

For this batch, run 100 total sequences as 10 blocks of 10 runs.
Keep the command shape the same unless the profile settings below change.

## Logging

- Capture one log file per run as `run_01.log` through `run_10.log`
- Capture the serial session transcript in `session.log`
- Keep the batch folder self-contained

## Notes

- Use the updated generic support labels in analyzer output.
- Do not compare against pasted console text alone; use the saved logs.
- When a run finishes, summarize misses, duplicates, late results, `avg_dt_ms`, and `avg_strength` before adjusting tuning.

## Suggested shift ladder

Use one block per shift and keep the previous block as the comparison baseline.

| Block | Max duration | Onset | Release | Release debounce | Notes |
| --- | --- | --- | --- | --- | --- |
| 1 | 300 | 20000 | 5000 | 30 | Baseline after the current flash |
| 2 | 280 | 20000 | 5000 | 30 | Tighten duration first |
| 3 | 260 | 20000 | 5000 | 30 | Keep watching for misses |
| 4 | 240 | 20000 | 5000 | 30 | Stop if duplicates reappear |
| 5 | 220 | 20000 | 5000 | 30 | Keep hysteresis unchanged |
| 6 | 220 | 20000 | 5000 | 20 | Reduce debounce if miss-free |
| 7 | 220 | 19000 | 4800 | 20 | Slightly lower entry/exit gate |
| 8 | 220 | 18000 | 4500 | 20 | Only if the previous block stayed clean |
| 9 | 200 | 18000 | 4500 | 20 | Go tighter only if duplicates stay at zero |
| 10 | 200 | 18000 | 4500 | 15 | Final tightening pass if still stable |

If a block gets worse, keep the last good values and use the next block to test a smaller adjustment instead of jumping again.

## Knob playbook

- `onsetDetectionThreshold`: move down if valid events are missed before opening; move up if weak noise opens too often.
- `onsetReleaseThreshold`: move down if peaks close too early; move up if release hangs too long.
- `cooldownAfterOnsetMs`: move up if the same acoustic event retriggers; move down if nearby valid events are being suppressed.
- `minTransientDurationMs`: move down if valid short bursts are rejected; move up if very short noise bursts pass through.
- `maxTransientDurationMs`: move down if long lingering peaks create duplicates or late cleanup; move up if valid events are being cut off.
- `minTransientPeakStrength`: move up if weak ambient peaks are accepted; move down if valid but weaker bursts are missed.
- `releaseDebounceMs`: move up if release chatters or duplicates appear; move down if real events are being held open too long.
