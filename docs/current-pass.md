# Current Pass

## Pass J

Status: in progress

Goal:
- compare actual DetectionRuntime results against the Analyzer-side recheck
- report mismatches explicitly in `SEQ_EXPLAIN`
- keep `SEQ_TRIAL` and `SEQ_SUMMARY` shapes stable

Verified:
- `platformio run -e esp32dev-analyzer`
- `platformio run -t upload -e esp32dev-analyzer`
- live `SEQ profile=ampstate tries=1 debug=2`

Observed on-device:
- `SEQ_EXPLAIN_PIPELINE_SOURCE source=actual_pipeline fallback=0`
- `SEQ_EXPLAIN_PARITY compared=1 match=0 locality_mismatch=1`
- `SEQ_TRIAL ... artifact_state=CAPTURED artifact_reason=captured_from_runtime_pipeline`

Next:
- Pass K removal or quarantine of Analyzer-side re-evaluation from the normal path
