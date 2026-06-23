# Current Pass - TonalPulseScalar Live Validation

## Goal

Validate the landed TonalPulseScalar scalar-quality path on real hardware.
The firmware-side refactor is already in place; this pass is about proving the
runtime behavior and keeping the remaining tuning honest.

## What We Are Verifying

- `ScalarTransientDetector` still owns the single-carrier lifecycle.
- Carrier quality gates stay isolated to the observed carrier stream.
- Inspector evidence remains split across contrast and AMP, with carrier
  quality staying inside the detector.
- `PatternMatcher` returns `confirmed`, `uncertain`, or `rejected` with the
  expected first-failed requirement details.
- `SEQ_TRIAL`, `SEQ_INSPECT`, and `SEQ_EXPLAIN` report the canonical values.
- `requireCarrierQuality=false` behaves like the older detector path.
- `requireCarrierQuality=true` produces quality-based rejects or uncertain
  results without introducing a second detection pipeline.

## Build And Flash

Known-good build checks:

- `pio run -e esp32dev-analyzer`
- `pio run -e esp32dev`

Current runtime state:

- `esp32dev-analyzer` flashes successfully on `COM6`
- run the live TonalPulseScalar sequence
- confirm the expected analyzer output on the actual board

Remaining validation:

- confirm live runtime output on the flashed board
- capture the observed 100 cm / 200 cm / 250 cm behavior
- keep tuning within the landed TonalPulseScalar profile only

## Live Checks

Use the live board to verify:

1. 100 cm runs still confirm reliably.
2. 200 cm runs are mostly `uncertain` or `rejected`.
3. 250 cm runs produce no `confirmed` hits.
4. `carrier.quality_pass` matches the detector gate state.
5. `contrast_class` and `amp_class` are reported in `SEQ_TRIAL`.
6. `contrast.class`, `amp.class`, and the first failed pattern requirement are
   reported in `SEQ_EXPLAIN`.

## If The Runtime Mismatches

Calibration note:

- the initial thresholds are deduced from RAW PCM captures, then mapped into
  the normalized scalar stream
- do not assume the starting numbers are automatically correct for the live
  board without measurement

Only retune within the existing profile boundaries:

- first loosen `minCoverageAboveReleaseMs` or `minLongestIslandMs` if true
  100 cm signals are getting lost
- first tighten carrier-quality gates or AMP thresholds if too many far-field
  cases still confirm
- do not reintroduce a multi-scalar detector
- do not move AMP or contrast ownership into the carrier detector

## Done Criteria For This Pass

- Live upload succeeds on the actual ESP board.
- The analyzer output matches the landed canonical fields.
- The observed 100 cm / 200 cm / 250 cm behavior is documented.
- Any remaining tuning needed by the live board is localized to the existing
  TonalPulseScalar profile.
