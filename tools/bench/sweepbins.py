"""Per-bin received level of every RAW hop capture in a band-sweep session, by position and round, then by absolute
frequency across the positions that cover it. Reads the hop bins from each capture's RAW_BEGIN line (hop=...), so it
works for any band; hopbins.py is the fixed-band version for step 5h cells.

    python tools/bench/sweepbins.py bench/sessions/2026-10-27-step11h-sweep24 [--min-round 2]

Levels are 10 log10(Goertzel power / samples) over the whole capture, in dB with an arbitrary reference: compare
bins with each other, not with another session's gain. A null shows as a bin 10 dB or more under its neighbours; a
null that drifts shows as a large round-to-round spread at one frequency."""
import csv
import glob
import math
import os
import re
import statistics
import sys

SR = 16000


def goertzel(x, f):
    k = 2.0 * math.cos(2.0 * math.pi * f / SR)
    s1 = s2 = 0.0
    for v in x:
        s0 = v + k * s1 - s2
        s2, s1 = s1, s0
    return s1 * s1 + s2 * s2 - k * s1 * s2


def read_capture(path):
    """Returns (hop bins from RAW_BEGIN, pcm samples)."""
    hops, pcm, col = [], [], None
    with open(path, encoding="utf-8", errors="replace") as f:
        for row in csv.reader(f):
            if not row:
                continue
            if col is None:
                if row[0].startswith("RAW_BEGIN"):
                    m = re.search(r"\bhop=([\d,]+)", ",".join(row))
                    if m:
                        hops = [int(h) for h in m.group(1).split(",") if h]
                elif row[0] == "ms" and "pcm" in row:
                    col = row.index("pcm")
                continue
            try:
                pcm.append(float(row[col]))
            except (ValueError, IndexError):
                pass
    return hops, pcm


def main(argv):
    if not argv or argv[0].startswith("-"):
        print(__doc__)
        return 2
    folder = argv[0]
    min_round = 1
    if "--min-round" in argv:
        min_round = int(argv[argv.index("--min-round") + 1])
    levels = {}  # (pos, round) -> {freq: dB}
    for path in sorted(glob.glob(os.path.join(folder, "pos*_r*_hop5_0.csv"))):
        m = re.search(r"pos(\d+)_r(\d+)_", os.path.basename(path))
        pos, rnd = int(m.group(1)), int(m.group(2))
        hops, pcm = read_capture(path)
        if not hops or len(pcm) < 1000:
            print("skipped %s (hops=%s samples=%d)" % (os.path.basename(path), hops, len(pcm)))
            continue
        n = len(pcm)
        levels[(pos, rnd)] = {f: 10.0 * math.log10(goertzel(pcm, f) / n + 1e-9) for f in hops}
    if not levels:
        print("no pos*_r*_hop5_0.csv in", folder)
        return 1
    positions = sorted({k[0] for k in levels})
    rounds = sorted({k[1] for k in levels})

    print("== level per bin (dB), one line per position and round")
    for pos in positions:
        for rnd in rounds:
            d = levels.get((pos, rnd))
            if d:
                print("%5d r%d  " % (pos, rnd) + "  ".join("%d:%5.1f" % (f, v) for f, v in sorted(d.items())))

    print("\n== per absolute bin, rounds >= %d: median over rounds +- (max - min), per position covering it" % min_round)
    freqs = sorted({f for d in levels.values() for f in d})
    for f in freqs:
        cells = []
        for pos in positions:
            vals = [levels[(pos, r)][f] for r in rounds if (pos, r) in levels and r >= min_round and f in levels[(pos, r)]]
            if vals:
                cells.append("%d:%5.1f+-%4.1f" % (pos, statistics.median(vals), max(vals) - min(vals)))
        print("%5d  " % f + "  ".join(cells))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
