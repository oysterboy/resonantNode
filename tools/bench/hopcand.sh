#!/bin/sh
# 5b candidate hop shapes at one spacing: 5-step 25 ms gap 10 and a bin-aligned 4-step 30 ms gap 10 (each survives one step in a null).
# usage: tools/bench/hopcand.sh COMPORT DISTANCE_CM SESSION_SLUG [extra note]
PORT=$1; CM=$2; SLUG=$3; NOTE=${4:-}
PY=~/.platformio/penv/Scripts/python.exe
$PY tools/bench/seqrun.py --port "$PORT" --session "bench/sessions/$SLUG" \
  --purpose "5b candidate hop shapes at $CM cm with the combined build (contrast medium 30500, amp medium 1500, history 512)" \
  --distance-cm "$CM" --orientation "mic and speaker facing up, bench" \
  --note "E2 Analyzer with -D FEATURE_HISTORY_BINS=512 -D SCALAR_AMP_MEDIUM_THRESHOLD=1500.0f -D SCALAR_CONTRAST_MEDIUM_THRESHOLD=30500.0f; E3 Emitter hop 0.5 FS; $NOTE" \
  --run chirp3200 "SEQ DIAG on;SEQ start profile=TonalPulseScalar tries=10 freq=3200 mode=detail when=all verbose=1" \
  --run hop5_p25g10 "SEQ DIAG on;SEQ start profile=TonalPulseScalar tries=10 freq=3300 band=2800-3800 dur=25 pulses=5 gap=10 hop=2800,3050,3300,3550,3800 mode=detail when=all verbose=1" \
  --run hop4_p30g10_bin "SEQ DIAG on;SEQ start profile=TonalPulseScalar tries=10 freq=3300 band=2800-3800 dur=30 pulses=4 gap=10 hop=2800,3300,3800,3050 mode=detail when=all verbose=1" 2>&1 | grep -E "done"
