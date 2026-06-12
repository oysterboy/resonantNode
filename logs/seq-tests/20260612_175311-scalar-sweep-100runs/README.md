# Scalar Sweep Batch

Status: block 1 starting

## Goal

- Run 10-run blocks
- Inspect each block before changing the next tuning step
- Keep misses at zero if possible
- Keep duplicates at zero if possible

## Baseline settings

- Profile: `TonalPulseScalar`
- Max duration: `300 ms`
- Onset threshold: `20000.0`
- Release threshold: `5000.0`
- Cooldown after onset: `50 ms`
- Min duration: `60 ms`
- Min peak strength: `0.0`
- Release debounce: `30 ms`

## Command

```text
SEQ start profile=TonalPulseScalar tries=50 mode=source when=miss verbose=1
```

## Block plan

- Block 1: baseline
- Block 2: shorten max duration if block 1 is clean
- Block 3: shorten again if the previous block stays clean
- Block 4: keep tightening duration or debounce only if misses stay at zero
- Block 5 through 10: continue small shifts, one block at a time

## Notes

- The block decision is made after each 10-run summary.
- The firmware must be flashed again whenever the profile knobs change.
