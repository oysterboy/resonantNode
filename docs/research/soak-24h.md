# 24-hour bench soak: does the room change signal and detection?

```text
Status:  idea (2026-10-09; folded in from issue #27, opened the same day)
Became:  nothing yet. Issue #27 is titled "[Step 1c]", but
         roadmap-0-steps.md has no step 1c; the owner decides whether it
         becomes a step or a roadmap item
Lab:     docs/lab/notes_lab.md 2026-10-09 (day-to-day level change at
         10 cm); bench:sessions/2026-10-09-issue26-10cm
```

## Question

Does the received signal or the detection outcome change with room
conditions over a day (temperature, humidity, background noise,
people/HVAC/traffic, daylight)? At 10 cm the TonalPulseFreq strength median
went from 17,300 (2026-10-08) to 12,800 (2026-10-09) at the same nominal
setup, and we can't tell whether the room or a bumped board caused it.

## Design (from #27, as written 2026-10-09)

- One unattended runner for 24 h, `tools/bench/soak.py` (to write, on top
  of `seqrun.py`). Every 15 min a short block: e.g. 10 x TonalPulseScalar,
  10 x TonalPulseFreq (with the documented override), one quiet noise-floor
  capture, one triggered RAW feature capture (peak amp, peak freq score).
- One bench session (`bench/sessions/<date>-soak24h-<distance>cm`): raw
  logs per block plus one `soak.csv` row per block (time, accept/expected/
  reject counts, strength median, peak freq score, noise floor,
  empty-history count, I2S backsteps).
- The runner reconnects after a serial error or board reset and needs
  nobody to keep going. Claude checks in rarely (start, ~+2 h, ~+12 h,
  end) and reads only the tail of `soak.csv`; the last check-in writes the
  session README (table by hour) and commits to `bench`.
- Room conditions recorded by hand in `session.json` notes (heating,
  windows, people, rough temperature). A sensor (e.g. BME280 on I2C) is out
  of scope unless one is at hand.

Gate proposed in #27: 24 h of blocks with at most a few missing,
`soak.csv` and logs on `bench`, and a written answer: does strength /
accept rate vary by more than block-to-block noise, and does it track time
of day or a noted condition.

## Still open (2026-10-09, Claude review)

- #27 planned the run on the piezo bench (Sunday 2026-10-11). Piezo was
  discontinued the same day (decisions/2026-10-09-discontinue-piezo.md);
  a soak on D-AMP measures the hardware the field trial will use.
- Prerequisite in #27 ("land #26 first") is done (e69aa09).
- Analyzer `mode=detail` drops ~22 DMA buffers per detected trial (ANA-004);
  the blocks should run in a mode without drops, or count them per block.
- Fits next to E001 phase 3 (`acoustic-test-suite.md`): the soak gives the
  block-to-block and hour-to-hour spread that any cell comparison has to
  beat.
