"""Analyzer A/B set (step 11g batch C's bench gate, docs/refactors/analyzer-run-trial-split.md): the same SEQ cases
on one Analyzer build, one transcript per case, so two builds' runs can be compared with seqcmp.py and by eye.

Cases: hop10, chirp 3200 / 100 ms, chirp 3450 / 150 ms (the room checks' set), and the edge cases batch C names:
SEQ start during a run, SEQ start + SEQ STOP in one write, SEQ SUMMARY before the first trial ends, a test= label
longer than 47 characters, SEQ start + SEQ MODE quiet in one write, and SEQ STATUS before and during a run.

  python tools/bench/analyzerab.py --port COM3 --out bench/sessions/<folder>/<label>
"""
import argparse
import os
import time

import serial

HOP = "freq=3450 band=2950-3950 dur=25 pulses=5 gap=10 hop=2950,3200,3450,3700,3950"
SEQ3 = "SEQ start profile=TonalPulseScalar tries=3 band=2950-3950 mode=detail when=all verbose=1"
LONG_LABEL = "ab_label_" + "x" * 60

# (name, [(delay_s_before_write, text)], wait_s after the last write)
CASES = [
    ("hop10", [(0, "SEQ start profile=TonalPulseScalar tries=10 %s mode=detail when=all verbose=1" % HOP)], 45),
    ("chirp3200_100", [(0, "SEQ start profile=TonalPulseScalar tries=10 dur=100 band=2950-3950 mode=detail when=all "
                           "verbose=1")], 45),
    ("chirp3450_150", [(0, "SEQ start profile=TonalPulseScalar tries=10 freq=3450 dur=150 band=2950-3950 "
                           "mode=detail when=all verbose=1")], 45),
    ("status_idle", [(0, "SEQ STATUS")], 2),
    ("start_during_run", [(0, SEQ3), (3, "SEQ STATUS"), (0.5, SEQ3)], 20),
    ("start_stop_one_write", [(0, SEQ3 + "\nSEQ STOP")], 8),
    ("summary_before_first_trial", [(0, SEQ3), (0.5, "SEQ SUMMARY")], 15),
    ("long_label", [(0, SEQ3 + " test=" + LONG_LABEL)], 15),
    ("start_mode_quiet_one_write", [(0, SEQ3 + "\nSEQ MODE quiet"), (3, "SEQ STATUS")], 15),
]

ap = argparse.ArgumentParser()
ap.add_argument("--port", required=True)
ap.add_argument("--out", required=True)
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)
s = serial.Serial()
s.port, s.baudrate, s.timeout = a.port, 115200, 0.05
s.dtr = False
s.rts = False
s.open()


def read_for(seconds):
    end = time.time() + seconds
    buf = b""
    while time.time() < end:
        buf += s.read(8192)
    return buf.decode("ascii", errors="replace")


read_for(1)
for name, writes, wait in CASES:
    text = []
    for delay, line in writes:
        if delay:
            text.append(read_for(delay))
        s.write((line + "\n").encode())
        text.append(">> " + line.replace("\n", "\\n") + "\n")
    text.append(read_for(wait))
    s.write(b"SEQ STOP\n")  # leave no run behind for the next case
    text.append(">> SEQ STOP (cleanup)\n" + read_for(2))
    open(os.path.join(a.out, name + ".log"), "w", encoding="utf-8").write("".join(text))
    print(name, "done", flush=True)
s.close()
