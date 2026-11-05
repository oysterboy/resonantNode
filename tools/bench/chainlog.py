"""Chains in a board view log (boardview.py --log, radio boards included): emits grouped into chains (a gap over
--gap s ends one), per chain its length, how many emits, how it started, and what the boards heard just after its
last emit (the decisions that ended it); then a summary.

  python tools/bench/chainlog.py LOG [LOG ...] [--gap 4] [--quiet E3] [--brief]

--quiet names boards to leave out of the heard side (a silent gateway decides outputs_off on everything).
"""
import argparse
import re
from collections import Counter

ap = argparse.ArgumentParser()
ap.add_argument("logs", nargs="+")
ap.add_argument("--gap", type=float, default=4.0)
ap.add_argument("--quiet", default="")
ap.add_argument("--brief", action="store_true", help="the summary only")
a = ap.parse_args()
quiet = set(filter(None, a.quiet.split(",")))

for path in a.logs:
    ev = []
    for line in open(path, encoding="utf-8", errors="replace"):
        p = line.rstrip("\n").split(" ", 2)
        if len(p) < 3:
            continue
        try:
            t = float(p[0])
        except ValueError:
            continue
        board, text = p[1], p[2]
        if text.startswith("RB emit accepted"):
            m = re.search(r"source=(\w+)", text)
            ev.append((t, board, "emit", m.group(1) if m else "?"))
        elif text.startswith("RB verdict=") and board not in quiet:
            d = re.search(r"decision=(\w+)", text)
            v = re.search(r"valid=(\d)", text)
            ev.append((t, board, "heard", (d.group(1) if d else "?") + ("" if v and v.group(1) == "1" else "/invalid")))
    ev.sort()
    chains, cur = [], []
    for e in ev:
        if e[2] != "emit":
            continue
        if cur and e[0] - cur[-1][0] > a.gap:
            chains.append(cur)
            cur = []
        cur.append(e)
    if cur:
        chains.append(cur)
    print("## %s: %d chains" % (path, len(chains)))
    ends = Counter()
    lengths = []
    for c in chains:
        last = c[-1][0]
        after = [(b, w) for t, b, k, w in ev if k == "heard" and last < t <= last + 3]
        for _, w in after:
            ends[w] += 1
        lengths.append(len(c))
        if not a.brief:
            print("  %8.1f  %5.1f s  %3d emits  start=%-6s  after: %s" % (
                c[0][0], last - c[0][0], len(c), c[0][3], ", ".join("%s:%s" % x for x in after) or "nothing heard"))
    if lengths:
        s = sorted(lengths)
        print("  emits per chain: median %d, max %d, mean %.1f; chains of 1: %d" % (
            s[len(s) // 2], s[-1], sum(s) / len(s), s.count(1)))
        print("  heard after a chain's last emit: %s" % dict(ends.most_common()))
