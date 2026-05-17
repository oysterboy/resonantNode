# Current Pass

## Pass I

Status: in progress

Goal:
- make Analyzer consume the actual DetectionRuntime result when available
- keep the Analyzer-side recheck as a fallback for now
- keep SEQ_TRIAL, SEQ_EXPLAIN, and SEQ_SUMMARY shapes stable

Verified:
- `platformio run -e esp32dev-analyzer`
- `platformio run -t upload -e esp32dev-analyzer`
- live `SEQ profile=ampstate tries=1 debug=2`

Observed on-device:
- `SEQ_EXPLAIN_PIPELINE_SOURCE source=actual_pipeline fallback=0`
- `SEQ_TRIAL ... artifact_state=CAPTURED artifact_reason=captured_from_runtime_pipeline`

Next:
- Pass J parity check between actual runtime result and analyzer recheck
- Pass K removal of Analyzer-side re-evaluation from the normal path
