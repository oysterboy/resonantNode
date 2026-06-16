# Block 06 Summary

Status: running

Last run: 12
Requested tune: PARAM scalar_max_duration_ms=220 scalar_onset_threshold=19000 scalar_release_threshold=4800 scalar_cooldown_ms=50 scalar_release_debounce_ms=20 scalar_min_duration_ms=60 scalar_min_peak_strength=0
Applied tune: PARAM scalar_observed_stream=FrequencyScore scalar_onset_threshold=19000.0 scalar_release_threshold=4800.0 scalar_cooldown_ms=50 scalar_min_duration_ms=60 scalar_max_duration_ms=220 scalar_min_peak_strength=0.0 scalar_release_debounce_ms=20
Summary: SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=43 early_trials=0 late_trials=3 miss_trials=4 duplicate_trials=5 unexpected_trials=0 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=46 detector_reject_trials=4 pattern_valid_trials=46 pattern_rejected_trials=0 avg_dt_ms=197 avg_strength=131107.4 avg_conf=1.00

## Run Summaries
- SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=42 early_trials=0 late_trials=2 miss_trials=5 duplicate_trials=4 unexpected_trials=1 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=45 detector_reject_trials=5 pattern_valid_trials=44 pattern_rejected_trials=0 avg_dt_ms=123 avg_strength=124015.5 avg_conf=1.00
- SEQ_SUMMARY profile=TonalPulseScalar detector=scalar_transient trials=50 completed=50 expected_trials=43 early_trials=0 late_trials=3 miss_trials=4 duplicate_trials=5 unexpected_trials=0 rejected_trials=0 buffer_overrun_trials=0 detector_accepted_trials=46 detector_reject_trials=4 pattern_valid_trials=46 pattern_rejected_trials=0 avg_dt_ms=197 avg_strength=131107.4 avg_conf=1.00

## Notes
- This campaign keeps the current scalar parameters unless a later tuning pass changes them.
- Applied tune is captured from the confirmed `PARAM STATUS` snapshot.
