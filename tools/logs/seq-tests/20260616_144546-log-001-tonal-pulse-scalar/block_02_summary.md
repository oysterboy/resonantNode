# Block 02 Summary

Status: running

Last run: 02
Requested tune: PARAM scalar_max_duration_ms=220 scalar_onset_threshold=19000 scalar_release_threshold=7500 scalar_cooldown_ms=50 scalar_release_debounce_ms=25 scalar_min_duration_ms=60 scalar_min_peak_strength=0
Applied tune: PARAM scalar_observed_stream=FrequencyTargetBand scalar_onset_threshold=19000.0 scalar_release_threshold=7500.0 scalar_cooldown_ms=50 scalar_min_duration_ms=60 scalar_max_duration_ms=220 scalar_min_peak_strength=0.0 scalar_release_debounce_ms=25
Summary: SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=45 early_trials=0 late_trials=1 miss_trials=4 duplicate_trials=3 unexpected_trials=0 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=46 detector_reject_trials=4 pattern_valid_trials=46 pattern_rejected_trials=0 avg_dt_ms=542 avg_strength=66465.3 avg_conf=1.00

## Run Summaries
- SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=45 early_trials=0 late_trials=1 miss_trials=4 duplicate_trials=3 unexpected_trials=0 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=46 detector_reject_trials=4 pattern_valid_trials=46 pattern_rejected_trials=0 avg_dt_ms=542 avg_strength=66465.3 avg_conf=1.00

## Notes
- This campaign keeps the current scalar parameters unless a later tuning pass changes them.
- Applied tune is captured from the confirmed `PARAM STATUS` snapshot.
