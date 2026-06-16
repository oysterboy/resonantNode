# Block 01 Summary

Status: running

Last run: 01
Requested tune: PARAM scalar_max_duration_ms=220 scalar_onset_threshold=19000 scalar_release_threshold=5000 scalar_cooldown_ms=50 scalar_release_debounce_ms=30 scalar_min_duration_ms=60 scalar_min_peak_strength=0
Applied tune: PARAM scalar_observed_stream=FrequencyTargetBand scalar_onset_threshold=19000.0 scalar_release_threshold=5000.0 scalar_cooldown_ms=50 scalar_min_duration_ms=60 scalar_max_duration_ms=220 scalar_min_peak_strength=0.0 scalar_release_debounce_ms=30
Summary: SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=39 early_trials=0 late_trials=1 miss_trials=10 duplicate_trials=1 unexpected_trials=0 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=40 detector_reject_trials=10 pattern_valid_trials=40 pattern_rejected_trials=0 avg_dt_ms=752 avg_strength=67386.5 avg_conf=1.00

## Run Summaries
- SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=39 early_trials=0 late_trials=1 miss_trials=10 duplicate_trials=1 unexpected_trials=0 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=40 detector_reject_trials=10 pattern_valid_trials=40 pattern_rejected_trials=0 avg_dt_ms=752 avg_strength=67386.5 avg_conf=1.00

## Notes
- This campaign keeps the current scalar parameters unless a later tuning pass changes them.
- Applied tune is captured from the confirmed `PARAM STATUS` snapshot.
