# Block 07 Summary

Status: running

Last run: 07
Requested tune: PARAM scalar_max_duration_ms=220 scalar_onset_threshold=20000 scalar_release_threshold=15000 scalar_cooldown_ms=50 scalar_release_debounce_ms=10 scalar_min_duration_ms=60 scalar_min_peak_strength=0
Applied tune: PARAM scalar_observed_stream=FrequencyTargetBand scalar_onset_threshold=20000.0 scalar_release_threshold=15000.0 scalar_cooldown_ms=50 scalar_min_duration_ms=60 scalar_max_duration_ms=220 scalar_min_peak_strength=0.0 scalar_release_debounce_ms=10
Summary: SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=37 early_trials=0 late_trials=7 miss_trials=6 duplicate_trials=15 unexpected_trials=0 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=44 detector_reject_trials=4 pattern_valid_trials=44 pattern_rejected_trials=0 avg_dt_ms=664 avg_strength=53721.8 avg_conf=1.00

## Run Summaries
- SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=37 early_trials=0 late_trials=7 miss_trials=6 duplicate_trials=15 unexpected_trials=0 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=44 detector_reject_trials=4 pattern_valid_trials=44 pattern_rejected_trials=0 avg_dt_ms=664 avg_strength=53721.8 avg_conf=1.00

## Notes
- This campaign keeps the current scalar parameters unless a later tuning pass changes them.
- Applied tune is captured from the confirmed `PARAM STATUS` snapshot.
