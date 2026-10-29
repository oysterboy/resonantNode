"""RAW capture set for 12.4.5: every mode, pre / decim / pulses / sweep / band, the clamp. One log per case.

  python rawset.py COM3 <outdir>
"""
import os
import sys
import time

import serial

CASES = [
    ("feat", "RAW trigger f=3300 dur=100 pre=100 post=300 mode=feat"),
    ("pcm", "RAW trigger f=3300 dur=100 pre=50 post=200 mode=pcm"),
    ("i2s_decim4", "RAW trigger f=3300 dur=100 post=300 decim=4 mode=i2s"),
    ("both_hop", "RAW trigger f=3450 dur=25 pre=100 post=300 mode=both pulses=5 gap=10 "
                 "hop=2950,3200,3450,3700,3950 band=2950-3950"),
    ("feat_sweep", "RAW trigger f=3000 dur=150 post=300 mode=feat sweep=3900"),
    ("both_nopre_clamp", "RAW trigger f=3300 dur=100 pre=0 post=99999 mode=both"),
]
port, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
s = serial.Serial()
s.port, s.baudrate, s.timeout = port, 115200, 0.05
s.dtr = False
s.rts = False
s.open()


def pump(sec, until=None):
    buf = ""
    end = time.time() + sec
    while time.time() < end:
        d = s.read(65536)
        if d:
            buf += d.decode(errors="replace")
            if until and until in buf:
                end = min(end, time.time() + 0.5)
                until = None
    return buf


pump(1)
for name, cmd in CASES:
    s.write((cmd + "\n").encode())
    text = pump(40, "RAW_SUMMARY")
    open(os.path.join(out, name + ".log"), "w", encoding="utf-8").write(text)
    rows = sum(1 for l in text.splitlines() if l[:1].isdigit())
    print("%-18s rows %5d  %s" % (name, rows, [l[:40] for l in text.splitlines() if l.startswith("RAW_")][:6]),
          flush=True)
    pump(1)
s.close()
