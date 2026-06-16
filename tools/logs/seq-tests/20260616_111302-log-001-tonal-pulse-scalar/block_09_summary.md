# Block 09 Summary

Status: running

Last run: 18
Requested tune: PARAM scalar_max_duration_ms=200 scalar_onset_threshold=17500 scalar_release_threshold=4200 scalar_cooldown_ms=50 scalar_release_debounce_ms=15 scalar_min_duration_ms=60 scalar_min_peak_strength=0
Applied tune: PARAM scalar_observed_stream=FrequencyScore scalar_onset_threshold=17500.0 scalar_release_threshold=4200.0 scalar_cooldown_ms=50 scalar_min_duration_ms=60 scalar_max_duration_ms=200 scalar_min_peak_strength=0.0 scalar_release_debounce_ms=15
Summary: SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=44 early_trials=0 late_trials=1 miss_trials=5 duplicate_trials=2 unexpected_trials=0 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=45 detector_reject_trials=5 pattern_valid_trials=45 pattern_rejected_trials=0 avg_dt_ms=59 avg_strength=135200.0 avg_conf=1.00

## Run Summaries
- SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=45 early_trials=0 late_trials=2 miss_trials=2 duplicate_trials=3 unexpected_trials=1 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=48 detector_reject_trials=2 pattern_valid_trials=47 pattern_rejected_trials=0 avg_dt_ms=91 avg_strength=130861.4 avg_conf=1.00
- SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=44 early_trials=0 late_trials=1 miss_trials=5 duplicate_trials=2 unexpected_trials=0 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=45 detector_reject_trials=5 pattern_valid_trials=45 pattern_rejected_trials=0 avg_dt_ms=59 avg_strength=135200.0 avg_conf=1.00

## Notes
- This campaign keeps the current scalar parameters unless a later tuning pass changes them.
- Applied tune is captured from the confirmed `PARAM STATUS` snapshot.
