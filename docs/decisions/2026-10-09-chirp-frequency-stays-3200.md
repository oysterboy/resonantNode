# The chirp stays at 3200 Hz on D-AMP

Status: decided.
Date: 2026-10-09.

## Decision

`CHIRP_FREQUENCY_HZ` stays 3200 for the D-AMP nodes.

## Why

Owner's call. The 110 cm sweep showed 4000-6400 Hz with 4-7 dB more
margin and the same false-positive result, but part of that is First
Difference's tilt (it goes if the preprocessor changes after the field
trial) and higher tones beam more narrowly; 3200 Hz already reaches
50/50 at 110 cm with 0.3 FS. Lower tones (300-2400 Hz) were worse.
Numbers: `docs/refactors/damp-bench-check.md` section 5.

## Rules out

Changing the chirp frequency as a range fix during steps 3-6.

## Revisit when

Range at 0.3 FS is not enough in the field trial, or after the
preprocessor pass (DC blocker), when the sweep should be rerun.

## Source

Owner, 2026-10-09, recommendation 2 ("no") in damp-bench-check.md section 6.
