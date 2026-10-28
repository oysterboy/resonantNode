#!/bin/sh
# 5b gate cells at one spacing: plain chirp, 3-step hop, 5-step hop (5 ms and 15 ms gaps).
# usage: tools/bench/gate5b.sh COMPORT DISTANCE_CM SESSION_SLUG [extra note]
# SPACE / LEVEL (env) set the session setup fields; defaults are what this battery ran with.
PORT=$1; CM=$2; SLUG=$3; NOTE=${4:-}
PY=~/.platformio/penv/Scripts/python.exe
$PY tools/bench/seqrun.py --port "$PORT" --session "bench/sessions/$SLUG" \
  --purpose "5b gate battery cells at $CM cm with the combined build (contrast medium 30500, amp medium 1500, history 512)" \
  --distance-cm "$CM" --orientation "mic and speaker facing up, bench" \
  --space "${SPACE:-desk}" --tone-level "${LEVEL:-0.5 FS}" \
  --note "E2 Analyzer with -D FEATURE_HISTORY_BINS=512 -D SCALAR_AMP_MEDIUM_THRESHOLD=1500.0f -D SCALAR_CONTRAST_MEDIUM_THRESHOLD=30500.0f; E3 Emitter hop 0.5 FS; $NOTE" \
  --run chirp3200 "SEQ DIAG on;SEQ start profile=TonalPulseScalar tries=10 freq=3200 mode=detail when=all verbose=1" \
  --run hop_p30g10 "SEQ DIAG on;SEQ start profile=TonalPulseScalar tries=10 freq=3300 band=2950-3950 dur=30 pulses=3 gap=10 hop=2950,3450,3950 mode=detail when=all verbose=1" \
  --run hop5_p20g5 "SEQ DIAG on;SEQ start profile=TonalPulseScalar tries=10 freq=3300 band=2950-3950 dur=20 pulses=5 gap=5 hop=2950,3200,3450,3700,3950 mode=detail when=all verbose=1" \
  --run hop5_p20g15_maxdur300 "PARAM scalar_max_duration_ms=300;SEQ DIAG on;SEQ start profile=TonalPulseScalar tries=10 freq=3300 band=2950-3950 dur=20 pulses=5 gap=15 hop=2950,3200,3450,3700,3950 mode=detail when=all verbose=1" 2>&1 | grep -E "done"
