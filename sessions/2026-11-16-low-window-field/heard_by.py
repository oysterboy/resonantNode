"""Who hears whom, from a boardview log: for each board's trains (RB emit shape=), the other boards' verdicts
within WIN s after. A train that overlaps a third board's train is left out (a chord would blur it).
Adapted from 2026-11-05-night-e0/heard_by.py: any board name, plus per-board totals of verdicts.

  python heard_by.py run1.log
"""
import collections
import re
import sys

WIN = 0.4
em = collections.defaultdict(list)  # board -> train start times
ver = collections.defaultdict(list)  # board -> (t, valid)
dec = collections.defaultdict(collections.Counter)  # board -> decision counts on valid verdicts
for l in open(sys.argv[1], errors="replace"):
    m = re.match(r"(\d+\.\d+) (\S+) (.*)", l)
    if not m:
        continue
    t, b, r = float(m.group(1)), m.group(2), m.group(3)
    if r.startswith("RB emit shape="):
        em[b].append(t)
    elif r.startswith("RB verdict="):
        v = re.search(r"valid=(\d)", r)
        ok = int(v.group(1)) if v else 0
        ver[b].append((t, ok))
        if ok:
            d = re.search(r"decision=(\w+)", r)
            dec[b][d.group(1) if d else "?"] += 1
boards = sorted(b for b in set(em) | set(ver) if b != "E0")
span = max([t for b in boards for t in em[b]] + [x[0] for b in boards for x in ver[b]] + [0])
print("log span %.0f s" % span)
for b in boards:
    n_ok = sum(x[1] for x in ver[b])
    print("%-4s trains %4d  verdicts %4d  valid %4d (%s)" % (b, len(em[b]), len(ver[b]), n_ok,
                                                            ", ".join("%s %d" % kv for kv in dec[b].most_common())))
print()
print("rows = sender, cols = listener: valid / clean trains (any verdict), within %.1f s" % WIN)
print("      " + "".join("%16s" % b for b in boards))
for e in boards:
    row = []
    for l in boards:
        if l == e or not em[e]:
            row.append("%16s" % "-")
            continue
        v = ok = anyv = 0
        for t in em[e]:
            if any(abs(t - u) < WIN for b2 in boards if b2 not in (e, l) for u in em[b2]):
                continue
            v += 1
            w = [x for x in ver[l] if t < x[0] <= t + WIN]
            anyv += bool(w)
            ok += any(x[1] for x in w)
        row.append("%16s" % ("%d/%d %d%% (%d)" % (ok, v, round(100 * ok / v), anyv) if v else "n/a"))
    print("%-6s" % e + "".join(row))
