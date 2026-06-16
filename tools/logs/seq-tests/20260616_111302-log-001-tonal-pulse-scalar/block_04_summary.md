# Block 04 Summary

Status: running

Last run: 08
Requested tune: PARAM scalar_max_duration_ms=240 scalar_onset_threshold=20000 scalar_release_threshold=5000 scalar_cooldown_ms=50 scalar_release_debounce_ms=25 scalar_min_duration_ms=60 scalar_min_peak_strength=0
Applied tune: PARAM scalar_observed_stream=FrequencyScore scalar_onset_threshold=20000.0 scalar_release_threshold=5000.0 scalar_cooldown_ms=50 scalar_min_duration_ms=60 scalar_max_duration_ms=240 scalar_min_peak_strength=0.0 scalar_release_debounce_ms=25
Summary: SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=49 early_trials=0 late_trials=0 miss_trials=1 duplicate_trials=1 unexpected_trials=0 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=49 detector_reject_trials=1 pattern_valid_trials=49 pattern_rejected_trials=0 avg_dt_ms=0 avg_strength=126568.7 avg_conf=1.00

## Run Summaries
- SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=49 early_trials=0 late_trials=0 miss_trials=1 duplicate_trials=2 unexpected_trials=0 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=49 detector_reject_trials=1 pattern_valid_trials=49 pattern_rejected_trials=0 avg_dt_ms=0 avg_strength=125249.5 avg_conf=1.00
- SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=49 early_trials=0 late_trials=0 miss_trials=1 duplicate_trials=1 unexpected_trials=0 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=49 detector_reject_trials=1 pattern_valid_trials=49 pattern_rejected_trials=0 avg_dt_ms=0 avg_strength=126568.7 avg_conf=1.00

## Notes
- This campaign keeps the current scalar parameters unless a later tuning pass changes them.
- Applied tune is captured from the confirmed `PARAM STATUS` snapshot.
