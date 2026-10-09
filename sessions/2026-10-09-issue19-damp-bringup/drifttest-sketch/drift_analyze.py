"""Band levels and drift per drifttest capture. Levels in dBFS of the 24-bit
word, framing-corrected (no-align captures read doubled, so halved here)."""
import cmath
import glob
import math
import os
import sys


def fft(x):
    n = len(x)
    if n == 1:
        return [x[0]]
    even, odd = fft(x[0::2]), fft(x[1::2])
    out = [0j] * n
    for k in range(n // 2):
        t = cmath.exp(-2j * math.pi * k / n) * odd[k]
        out[k] = even[k] + t
        out[k + n // 2] = even[k] - t
    return out


def load(path):
    hdr, pcm, rows = {}, [], False
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.strip()
        if line.startswith("RAW_BEGIN"):
            hdr = dict(t.split("=", 1) for t in line.split()[1:] if "=" in t)
        elif line == "ms,pcm":
            rows = True
        elif line.startswith("RAW_SUMMARY"):
            rows = False
        elif rows and "," in line:
            pcm.append(int(line.split(",")[1]))
    return hdr, pcm


FS = 8388607.0
BANDS = [(1, 30), (30, 100), (100, 300), (300, 1000), (1000, 3000), (3000, 8000)]
print(f"{'capture':24s} {'sr':>5s} {'dc':>8s} {'drift/s':>9s} {'ac dBFS':>8s} {'diff dBFS':>9s} " +
      " ".join(f"{lo}-{hi}Hz".rjust(10) for lo, hi in BANDS) + "  LF-HF dB")
for path in sorted(glob.glob(os.path.join(sys.argv[1], "*.log"))):
    hdr, x = load(path)
    sr = float(hdr["sr"])
    scale = 1.0 if hdr.get("align") == "1" else 0.5
    x = [v * scale for v in x]
    n = len(x)
    dc = sum(x) / n
    t = [i / sr for i in range(n)]
    tm = sum(t) / n
    slope = sum((ti - tm) * (v - dc) for ti, v in zip(t, x)) / sum((ti - tm) ** 2 for ti in t)
    ac = math.sqrt(sum((v - dc) ** 2 for v in x) / n)
    d = [(x[i] - x[i - 1]) / 2 for i in range(1, n)]
    diff_rms = math.sqrt(sum(v * v for v in d) / len(d))
    win = [0.5 - 0.5 * math.cos(2 * math.pi * i / (n - 1)) for i in range(n)]
    spec = fft([complex((v - dc) * w) for v, w in zip(x, win)])
    # Hann power normalisation so band power sums to the signal's mean square.
    norm = sum(w * w for w in win) * n / 2.0
    bands = []
    for lo, hi in BANDS:
        p = sum(abs(spec[k]) ** 2 for k in range(1, n // 2) if lo <= k * sr / n < hi) / norm
        bands.append(10 * math.log10(p / FS ** 2) if p > 0 else float("nan"))
    lf_hf = bands[2] - bands[4]
    db = lambda v: 20 * math.log10(v / FS) if v > 0 else float("nan")
    name = os.path.basename(path)[:-4]
    print(f"{name:24s} {sr/1000:4.0f}k {dc:8.0f} {slope:9.0f} {db(ac):8.1f} {db(diff_rms):9.1f} " +
          " ".join(f"{b:10.1f}" for b in bands) + f"  {lf_hf:7.1f}")
