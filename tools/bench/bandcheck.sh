#!/bin/sh
# Band 2950-3950 check at one spacing (decisions/2026-10-16-network-band-2950-3950.md): the plain
# 3,200 Hz chirp heard in band mode, the 5-step hop on the new bins, and the chirp on the single
# target as the reference. Works on any Analyzer build with SEQ band= (hop frequencies are Emitter-side).
# usage: tools/bench/bandcheck.sh COMPORT DISTANCE_CM SESSION_SLUG [extra note]
PORT=$1; CM=$2; SLUG=$3; NOTE=${4:-}
PY=~/.platformio/penv/Scripts/python.exe
$PY tools/bench/seqrun.py --port "$PORT" --session "bench/sessions/$SLUG" \
  --purpose "band 2950-3950 check at $CM cm: chirp 3200 in band mode, hop5 25x5 gap 10 on bins 2950/3200/3450/3700/3950, chirp 3200 single target" \
  --distance-cm "$CM" --orientation "mic and speaker facing up, bench" \
  --note "E3 Emitter hop 0.5 FS; $NOTE" \
  --run chirp3200_band "PARAM scalar_max_duration_ms=300;SEQ DIAG on;SEQ start profile=TonalPulseScalar tries=10 freq=3200 band=2950-3950 mode=detail when=all verbose=1" \
  --run hop5_p25g10_band "PARAM scalar_max_duration_ms=300;SEQ DIAG on;SEQ start profile=TonalPulseScalar tries=10 freq=3200 band=2950-3950 dur=25 pulses=5 gap=10 hop=2950,3200,3450,3700,3950 mode=detail when=all verbose=1" \
  --run chirp3200_single "PARAM scalar_max_duration_ms=300;SEQ DIAG on;SEQ start profile=TonalPulseScalar tries=10 freq=3200 mode=detail when=all verbose=1" 2>&1 | grep -E "done"
