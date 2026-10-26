"""Per-bin received level of hop captures (step 5h cells): Goertzel power at each hop bin over the whole capture, in dB re the
strongest bin, averaged over the reps of a session folder. One number per bin, so a null shows directly."""
import csv
import os
import glob
import math
import sys

BINS = [2950, 3200, 3450, 3700, 3950]
SR = 16000


def goertzel(x, f):
    k = 2.0 * math.cos(2.0 * math.pi * f / SR)
    s1 = s2 = 0.0
    for v in x:
        s0 = v + k * s1 - s2
        s2, s1 = s1, s0
    return s1 * s1 + s2 * s2 - k * s1 * s2


def pcm(path):
    out = []
    with open(path, encoding="utf-8", errors="replace") as f:
        header = None
        for row in csv.reader(f):
            if header is None:
                if row and row[0] == "ms" and "pcm" in row:
                    header = row.index("pcm")
                continue
            try:
                out.append(float(row[header]))
            except (ValueError, IndexError):
                pass
    return out


# python tools/bench/hopbins.py bench/sessions/<cell-a> bench/sessions/<cell-b>
for folder in sys.argv[1:]:
    files = sorted(glob.glob(folder + "/" + os.environ.get("HOPBINS_GLOB", "hop5_cell_r*.csv")))
    acc = [0.0] * len(BINS)
    for p in files:
        x = pcm(p)
        for i, b in enumerate(BINS):
            acc[i] += goertzel(x, b)
    top = max(acc)
    print("%-55s %s" % (folder.split("/")[-1], "  ".join("%d:%5.1f dB" % (b, 10 * math.log10(a / top)) for b, a in zip(BINS, acc))))
