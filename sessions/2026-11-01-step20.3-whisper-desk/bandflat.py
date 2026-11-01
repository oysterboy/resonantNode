"""The board's planned flatness (8 RBJ bandpass biquads 400-6000 Hz, energy / centre per 128-sample block, flatness of
the loud blocks) on the WHISPER REC recordings, to check it separates like the FFT flatness."""
import glob
import os
import sys
import wave

import numpy as np
from scipy import signal

RATE = 24000
N = 8
CENTRES = np.geomspace(300, 10000, N)
Q = 2.5


def coeffs(f0):
    w0 = 2 * np.pi * f0 / RATE
    alpha = np.sin(w0) / (2 * Q)
    a0 = 1 + alpha
    return np.array([alpha, 0, -alpha]) / a0, np.array([1, -2 * np.cos(w0) / a0, (1 - alpha) / a0])


FILTERS = [coeffs(f) for f in CENTRES]


def block_flatness(x, onset=250.0, block=128, shift=5):
    # the board's envelope: |x| (24-bit scale) one-pole with shift 5, Strength16 = env >> 8
    x24 = x * 256.0
    env = np.zeros_like(x24)
    e = 0.0
    k = 1.0 / (1 << shift)
    for i, v in enumerate(np.abs(x24)):
        e += (v - e) * k
        env[i] = e
    level = np.minimum(env, 262143.0) * 32767.0 / 262143.0
    bands = np.array([signal.lfilter(b, a, x) ** 2 for b, a in FILTERS])
    out = []
    for i in range(0, len(x) - block, block):
        if level[i + block - 1] < onset:
            continue
        E = bands[:, i:i + block].sum(axis=1) / CENTRES + 1e-9
        out.append(np.exp(np.mean(np.log(E))) / np.mean(E))
    return out


if __name__ == "__main__":
    for d in sys.argv[1:]:
        cls, path = d.split("=")
        vals = []
        for p in sorted(glob.glob(os.path.join(path, "rec", "*.wav"))):
            with wave.open(p) as w:
                x = np.frombuffer(w.readframes(w.getnframes()), "<i2").astype(float)
            f = block_flatness(x)
            if not f:
                print(cls, os.path.basename(p), "no loud block")
                continue
            med = float(np.median(f))
            vals.append(med)
            print("%-8s %-14s blocks=%3d median=%.2f p10=%.2f p90=%.2f" % (cls, os.path.basename(p), len(f), med,
                                                                         np.percentile(f, 10), np.percentile(f, 90)))
        print("%-8s MEDIANS %s" % (cls, " ".join("%.2f" % v for v in sorted(vals))))
