# Current Pass

## Pass K

Status: in progress

Goal:
- remove or quarantine Analyzer-side re-evaluation from the normal path
- keep `SEQ_TRIAL`, `SEQ_EXPLAIN`, and `SEQ_SUMMARY` stable
- keep the runtime pipeline as the source of truth

Verified:
- `platformio run -e esp32dev-analyzer`
- `platformio run -t upload -e esp32dev-analyzer`
- live `SEQ profile=ampstate tries=1 debug=2`

Observed on-device:
- `SEQ_EXPLAIN_PIPELINE_SOURCE source=actual_pipeline fallback=0`
- `SEQ_TRIAL ... artifact_state=CAPTURED artifact_reason=captured_from_runtime_pipeline`
- `SEQ_EXPLAIN ...` stays runtime-backed without public parity output

Next:
- Pass L only if shared reporting extraction becomes necessary later
