"""Candidate whisper features on board recordings (WHISPER REC): what separates room triggers from whispers and speech.

  recfeat.py room=<dir>[,<dir>] whisper=<dir> speech=<dir> [--skip name,name]
"""
import glob
import os
import sys
import wave

import numpy as np
from scipy import signal


def load(p):
    with wave.open(p) as w:
        x = np.frombuffer(w.readframes(w.getnframes()), "<i2").astype(float)
        return w.getframerate(), x - x.mean()


def feats(rate, x):
    f, t, S = signal.spectrogram(x, rate, nperseg=512, noverlap=256)
    P = S.mean(axis=1)
    tot = P[(f >= 100)].sum() + 1e-12
    bands = [(100, 300), (300, 1000), (1000, 3000), (3000, 6000), (6000, 11000)]
    frac = [P[(f >= a) & (f < b)].sum() / tot for a, b in bands]
    sel = (f >= 300) & (f < 6000)
    flat = np.exp(np.mean(np.log(P[sel] + 1e-12))) / (P[sel].mean() + 1e-12)
    # voicing: normalized autocorrelation peak at 80-400 Hz pitch, over 40 ms frames, the loudest half
    n = int(0.04 * rate)
    vs, es = [], []
    for i in range(0, len(x) - n, n // 2):
        fr = x[i:i + n] * np.hanning(n)
        e = (fr ** 2).sum()
        ac = np.correlate(fr, fr, "full")[n - 1:]
        lo, hi = int(rate / 400), int(rate / 80)
        vs.append(ac[lo:hi].max() / (ac[0] + 1e-12))
        es.append(e)
    vs, es = np.array(vs), np.array(es)
    loud = es >= np.median(es)
    voicing = float(np.median(vs[loud])) if loud.any() else 0.0
    # envelope at 8 ms: syllable peaks (local maxima 2x above the surrounding minima), crest
    env = np.sqrt(signal.sosfiltfilt(signal.butter(2, 30, fs=rate, output="sos"), x ** 2).clip(0))
    e8 = env[:: int(0.008 * rate)]
    peaks, _ = signal.find_peaks(e8, prominence=0.5 * np.median(e8) + 1e-9, distance=10)
    crest = e8.max() / (np.median(e8) + 1e-9)
    return frac, flat, voicing, len(peaks), crest


args = dict(a.split("=", 1) for a in sys.argv[1:] if "=" in a and not a.startswith("--"))
skip = set(sys.argv[sys.argv.index("--skip") + 1].split(",")) if "--skip" in sys.argv else set()
print("%-8s %-16s %5s %5s %5s %5s %5s | %5s %5s %3s %5s" % ("class", "file", "<300", "-1k", "-3k", "-6k", "-11k",
                                                             "flat", "voic", "pk", "crest"))
rows = {}
for cls, dirs in args.items():
    for d in dirs.split(","):
        for p in sorted(glob.glob(os.path.join(d, "rec", "*.wav"))):
            name = os.path.basename(p)
            if name in skip:
                continue
            rate, x = load(p)
            if len(x) < rate * 0.5:
                continue
            frac, flat, voic, pk, crest = feats(rate, x)
            rows.setdefault(cls, []).append((frac, flat, voic, pk, crest))
            print("%-8s %-16s %5.2f %5.2f %5.2f %5.2f %5.2f | %5.2f %5.2f %3d %5.1f" % (
                cls, name, *frac, flat, voic, pk, crest))
print()
for cls, rs in rows.items():
    a = np.array([[*r[0], r[1], r[2], r[3], r[4]] for r in rs])
    print("%-8s median " % cls + " ".join("%5.2f" % v for v in np.median(a, axis=0)))
