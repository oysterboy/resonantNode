# Block 07 Summary

Status: running

Last run: 14
Requested tune: PARAM scalar_max_duration_ms=220 scalar_onset_threshold=18000 scalar_release_threshold=4500 scalar_cooldown_ms=50 scalar_release_debounce_ms=20 scalar_min_duration_ms=60 scalar_min_peak_strength=0
Applied tune: PARAM scalar_observed_stream=FrequencyScore scalar_onset_threshold=18000.0 scalar_release_threshold=4500.0 scalar_cooldown_ms=50 scalar_min_duration_ms=60 scalar_max_duration_ms=220 scalar_min_peak_strength=0.0 scalar_release_debounce_ms=20
Summary: SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=45 early_trials=0 late_trials=0 miss_trials=5 duplicate_trials=0 unexpected_trials=0 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=45 detector_reject_trials=5 pattern_valid_trials=45 pattern_rejected_trials=0 avg_dt_ms=1 avg_strength=136080.3 avg_conf=1.00

## Run Summaries
- SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=47 early_trials=0 late_trials=0 miss_trials=3 duplicate_trials=1 unexpected_trials=0 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=47 detector_reject_trials=3 pattern_valid_trials=47 pattern_rejected_trials=0 avg_dt_ms=0 avg_strength=140745.8 avg_conf=1.00
- SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=45 early_trials=0 late_trials=0 miss_trials=5 duplicate_trials=0 unexpected_trials=0 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=45 detector_reject_trials=5 pattern_valid_trials=45 pattern_rejected_trials=0 avg_dt_ms=1 avg_strength=136080.3 avg_conf=1.00

## Notes
- This campaign keeps the current scalar parameters unless a later tuning pass changes them.
- Applied tune is captured from the confirmed `PARAM STATUS` snapshot.
