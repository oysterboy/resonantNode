"""Per 1 s window (0.5 s hop): power above the room's own spectrum (silence.wav), the excess's shares per band,
its centroid, and the envelope's 2-8 Hz modulation share in 1-4 kHz. Windows with < 3 dB excess are skipped."""
import os
import sys
import wave

import numpy as np
from scipy import signal

d = sys.argv[1]
skip = {"whisper_05m": [(9.0, 17.0)]}  # the phone notification


def load(n):
    with wave.open(os.path.join(d, n + ".wav")) as w:
        return w.getframerate(), np.frombuffer(w.readframes(w.getnframes()), "<i2").astype(float)


rate, sil = load("silence")
f, room = signal.welch(sil, rate, nperseg=512)
BANDS = [(300, 1000), (1000, 2500), (2500, 4500), (4500, 8000)]
use = (f >= 300) & (f < 8000)
sos = signal.butter(4, [1000, 4000], btype="band", fs=rate, output="sos")

print("%-12s %3s %6s  %-5s %-5s %-5s %-5s  %6s  %5s" % ("label", "win", "exdB", "0.3-1", "1-2.5", "2.5-4", "4.5-8",
                                                        "centr", "mod"))
for name in ["whisper_05m", "whisper_1m", "whisper_2m", "speech_1m", "typing", "chair", "paper", "clothes"]:
    r, x = load(name)
    rows = []
    for i in range(0, len(x) - rate, rate // 2):
        t = i / rate
        if any(a <= t < b for a, b in skip.get(name, [])):
            continue
        w = x[i:i + rate]
        _, P = signal.welch(w, rate, nperseg=512)
        ex = np.clip(P - room, 0, None)
        exdb = 10 * np.log10(P[use].sum() / room[use].sum())
        if exdb < 3:
            continue
        shares = [ex[(f >= a) & (f < b)].sum() for a, b in BANDS]
        tot = sum(shares) + 1e-12
        centr = (ex[use] * f[use]).sum() / (ex[use].sum() + 1e-12)
        env = np.abs(signal.sosfilt(sos, w))
        env = signal.resample_poly(env, 1, rate // 100)  # 100 Hz envelope
        env = env - env.mean()
        fm, M = signal.periodogram(env, 100)
        mod = M[(fm >= 2) & (fm <= 8)].sum() / (M[(fm > 0.5) & (fm <= 25)].sum() + 1e-12)
        rows.append([exdb] + [s / tot for s in shares] + [centr, mod])
    if not rows:
        print("%-12s   0" % name)
        continue
    m = np.median(np.array(rows), axis=0)
    print("%-12s %3d %6.1f  %.2f  %.2f  %.2f  %.2f  %6.0f  %5.2f" % (name, len(rows), *m))
