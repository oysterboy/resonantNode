# Block 10 Summary

Status: running

Last run: 20
Requested tune: PARAM scalar_max_duration_ms=220 scalar_onset_threshold=19000 scalar_release_threshold=1000 scalar_cooldown_ms=50 scalar_release_debounce_ms=10 scalar_min_duration_ms=60 scalar_min_peak_strength=0
Applied tune: PARAM scalar_observed_stream=FrequencyScore scalar_onset_threshold=19000.0 scalar_release_threshold=1000.0 scalar_cooldown_ms=50 scalar_min_duration_ms=60 scalar_max_duration_ms=220 scalar_min_peak_strength=0.0 scalar_release_debounce_ms=10
Summary: SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=0 early_trials=0 late_trials=0 miss_trials=9 duplicate_trials=0 unexpected_trials=41 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=41 detector_reject_trials=9 pattern_valid_trials=0 pattern_rejected_trials=0 avg_dt_ms=-1 avg_strength=0.0 avg_conf=0.00

## Run Summaries
- SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=0 early_trials=0 late_trials=0 miss_trials=3 duplicate_trials=0 unexpected_trials=47 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=47 detector_reject_trials=3 pattern_valid_trials=0 pattern_rejected_trials=0 avg_dt_ms=-1 avg_strength=0.0 avg_conf=0.00
- SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=0 early_trials=0 late_trials=0 miss_trials=9 duplicate_trials=0 unexpected_trials=41 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=41 detector_reject_trials=9 pattern_valid_trials=0 pattern_rejected_trials=0 avg_dt_ms=-1 avg_strength=0.0 avg_conf=0.00

## Notes
- This campaign keeps the current scalar parameters unless a later tuning pass changes them.
- Applied tune is captured from the confirmed `PARAM STATUS` snapshot.
