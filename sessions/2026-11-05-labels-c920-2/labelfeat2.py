"""Per label WAV in a labelrec directory (silence.wav = the room): over 0.5 s windows whose 1.5-4.5 kHz band stands at
least --min-mid dB over the room's, the median and 10/90th percentiles of that excess, the 200-700 Hz excess and the
5-8 kHz share. Usage: labelfeat2.py DIR [--min-mid 3]"""
import argparse
import os
import wave

import numpy as np
from scipy import signal

ap = argparse.ArgumentParser()
ap.add_argument("dir")
ap.add_argument("--min-mid", type=float, default=3.0)
a = ap.parse_args()


def load(n):
    with wave.open(os.path.join(a.dir, n)) as w:
        return w.getframerate(), np.frombuffer(w.readframes(w.getnframes()), "<i2").astype(float)


rate, sil = load("silence.wav")
f, room = signal.welch(sil, rate, nperseg=512)


def db(P, lo, hi):
    m = (f >= lo) & (f < hi)
    return 10 * np.log10(P[m].sum() / room[m].sum())


print("%-24s %4s  %-14s %-14s %-12s" % ("label", "win", "mid 1.5-4.5k", "low 200-700", "5-8k - mid"))
for n in sorted(x for x in os.listdir(a.dir) if x.endswith(".wav") and not x.startswith("cue") and x != "silence.wav"):
    r, x = load(n)
    rows = []
    for i in range(0, len(x) - r // 2, r // 4):
        _, P = signal.welch(x[i:i + r // 2], r, nperseg=512)
        mid = db(P, 1500, 4500)
        if mid < a.min_mid:
            continue
        rows.append((mid, db(P, 200, 700), db(P, 5000, 8000) - mid))
    rows = np.array(rows) if rows else np.zeros((0, 3))
    q = lambda c: "%4.0f %4.0f %4.0f" % tuple(np.percentile(rows[:, c], [10, 50, 90])) if len(rows) else "-"
    print("%-24s %4d  %-14s %-14s %-12s" % (n[:-4], len(rows), q(0), q(1), q(2)))
