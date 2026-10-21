#!/bin/sh
# 5b gate on a second day, shipped builds: plain 3,200 Hz chirp and the Node's hop train (25 ms x 5, gap 10, band bins).
# E2 Analyzer (esp32dev-analyzer, shipped profile values), E3 Emitter at 0.5 FS (-D I2S_TONE_AMPLITUDE=0.5f).
# usage: tools/bench/hopday2.sh COMPORT DISTANCE_CM SESSION_SLUG FIRMWARE_NOTE [extra note]
PORT=$1; CM=$2; SLUG=$3; FW=$4; NOTE=${5:-}
PY=~/.platformio/penv/Scripts/python.exe
$PY tools/bench/seqrun.py --port "$PORT" --session "bench/sessions/$SLUG" \
  --purpose "5b gate, second day: chirp and hop5 at $CM cm with the shipped builds" \
  --distance-cm "$CM" --orientation "mic and speaker facing up, bench" \
  --note "E2 Analyzer shipped build; E3 Emitter hop 0.5 FS; $FW; $NOTE" \
  --run chirp3200 "SEQ DIAG on;SEQ start profile=TonalPulseScalar tries=10 freq=3200 mode=detail when=all verbose=1" \
  --run hop5_p25g10 "SEQ DIAG on;SEQ start profile=TonalPulseScalar tries=10 freq=3300 band=2950-3950 dur=25 pulses=5 gap=10 hop=2950,3200,3450,3700,3950 mode=detail when=all verbose=1" 2>&1 | grep -E "done|ERR|rror"
