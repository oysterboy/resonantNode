#!/bin/sh
# Sweep inside one chirp (OUT-005, Emitter CHIRP sweep=) at one spacing: up 2950->3950, down 3950->2950,
# and the plain 3,200 Hz chirp as the reference, all in band mode. Needs an Emitter build with sweep=
# (9664032 or later) and an Analyzer that forwards SEQ start sweep=.
# usage: tools/bench/sweepcheck.sh COMPORT DISTANCE_CM SESSION_SLUG [extra note]
# SPACE / LEVEL (env) set the session setup fields; defaults are what this battery ran with.
PORT=$1; CM=$2; SLUG=$3; NOTE=${4:-}
PY=~/.platformio/penv/Scripts/python.exe
$PY tools/bench/seqrun.py --port "$PORT" --session "bench/sessions/$SLUG" \
  --purpose "sweep check at $CM cm: 100 ms glide 2950->3950 and 3950->2950 inside one chirp, plain 3200 chirp reference, band 2950-3950" \
  --distance-cm "$CM" --orientation "mic and speaker facing up, bench" \
  --space "${SPACE:-desk}" --tone-level "${LEVEL:-0.5 FS}" \
  --note "E3 Emitter with sweep= at 0.5 FS; $NOTE" \
  --run sweep_up "SEQ DIAG on;SEQ start profile=TonalPulseScalar tries=10 freq=2950 dur=100 sweep=3950 band=2950-3950 mode=detail when=all verbose=1" \
  --run sweep_down "SEQ DIAG on;SEQ start profile=TonalPulseScalar tries=10 freq=3950 dur=100 sweep=2950 band=2950-3950 mode=detail when=all verbose=1" \
  --run sweep_up_150 "SEQ DIAG on;SEQ start profile=TonalPulseScalar tries=10 freq=2950 dur=150 sweep=3950 band=2950-3950 mode=detail when=all verbose=1" \
  --run chirp3200_band "SEQ DIAG on;SEQ start profile=TonalPulseScalar tries=10 freq=3200 band=2950-3950 mode=detail when=all verbose=1" 2>&1 | grep -E "done"
