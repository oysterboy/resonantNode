# SEQ sample dump: default step 4 ms, 256-row buffer kept

Status: decided, implemented.
Date: 2026-10-13.

## Decision

The default `sampleStep` is 4 ms. The default window (lead 50 ms, tail
800 ms) then takes 213 rows and fits the unchanged 256-row (2 KB)
per-trial buffer. `sampleMax` is a budget over all dumped trials and is no
longer clamped to the per-trial buffer.

## Why

At 1 ms the default dump needed 851 rows and was refused every time.
Clamping `sampleMax` to 256 also refused any dump of two default trials.
At 4 ms the full window keeps the release, needs no extra RAM, and is
still finer than the detector's own timing (release debounce 10-30 ms).
The alternative was a 1024-row buffer: 8 KB more RAM in the Analyzer build.

## Rules out

1 ms default resolution; a larger buffer by default.

## Revisit when

A study needs 1 ms curves over long windows. Then pass `sampleStep=1`
with a shorter `sampleTail`, or grow the buffer for that build.

## Source

Owner, 2026-10-13, issue #32 (option 2a); issue #31 B12.
