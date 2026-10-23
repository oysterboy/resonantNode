"""Compare E1's emission shapes in two nodegroup logs (before / after step 10f)."""
import re
import sys
from collections import Counter


def load(path):
    rows = []
    for line in open(path, encoding="utf-8", errors="replace"):
        if line.startswith("#"):
            continue
        m = re.match(r"([\d.]+) (E\d) (.*)", line.rstrip("\n"))
        if m:
            rows.append((float(m.group(1)), m.group(2), m.group(3)))
    return rows


def report(path):
    rows = load(path)
    shapes = Counter()
    gaps = []
    heard_answer = [0, 0]
    heard_idle = [0, 0]
    last_idle = None
    e4_verdicts = [t for t, n, l in rows if n == "E4" and l.startswith("RB verdict=") and "valid=1" in l]
    status = [l for t, n, l in rows if n == "E1" and l.startswith("RB STATUS")]
    for t, n, l in rows:
        if n != "E1":
            continue
        if l.startswith("RB emit shape=idle_call"):
            last_idle = t
            shapes[l] += 1
        elif l.startswith("RB emit shape=hop"):
            shapes[re.sub(r"order=\S+", "order=*", l)] += 1
            heard = any(0.05 <= v - t <= 0.8 for v in e4_verdicts)
            if last_idle is not None and 0.4 <= t - last_idle <= 1.2:
                gaps.append(round(t - last_idle, 2))
                heard_idle[0] += heard
                heard_idle[1] += 1
                last_idle = None
            else:
                heard_answer[0] += heard
                heard_answer[1] += 1
        elif l.startswith("RB emit rejected"):
            shapes[l] += 1
    orders = Counter(m.group(1) for t, n, l in rows if n == "E1" for m in [re.search(r"shape=hop.*order=(\S+)", l)] if m)
    print(path)
    for k, v in sorted(shapes.items()):
        print("  %3d  %s" % (v, k))
    print("  orders:", dict(orders))
    print("  idle call: announce line -> train line (s):", sorted(gaps))
    print("  E4 heard E1's answer trains %d/%d, idle-call trains %d/%d" % (heard_answer[0], heard_answer[1], heard_idle[0], heard_idle[1]))
    if status:
        fields = re.findall(r"output\.\w+=\S+", status[-1])
        print("  last RB STATUS output.*:", " ".join(fields) if fields else "(none: before 10f)")


for p in sys.argv[1:]:
    report(p)
