"""Who hears whom: for each board's emissions (RB emit shape), the other boards' verdicts within WIN s after."""
import re, sys, collections
f = sys.argv[1]
WIN = 0.4
em = collections.defaultdict(list)   # board -> emit times
ver = collections.defaultdict(list)  # board -> (t, valid)
for l in open(f, errors="replace"):
    m = re.match(r"(\d+\.\d+) (E\d) (.*)", l)
    if not m:
        continue
    t, b, r = float(m.group(1)), m.group(2), m.group(3)
    if r.startswith("RB emit shape="):
        em[b].append(t)
    elif r.startswith("RB verdict="):
        v = re.search(r"valid=(\d)", r)
        ver[b].append((t, int(v.group(1)) if v else 0))
boards = sorted(set(em) | set(ver))
print("emits per board:", {b: len(em[b]) for b in boards})
print("rows = emitter, cols = listener: share of emits followed by a VALID verdict (any verdict) within %.1f s" % WIN)
print("      " + "".join("%14s" % b for b in boards))
for e in boards:
    row = []
    for l in boards:
        if l == e or not em[e]:
            row.append("%14s" % "-")
            continue
        v = ok = anyv = 0
        for t in em[e]:
            # skip emits that overlap another board's emit (a chord would blur it)
            if any(abs(t - u) < WIN for b2 in boards if b2 not in (e, l) for u in em[b2]):
                continue
            v += 1
            w = [x for x in ver[l] if t < x[0] <= t + WIN]
            anyv += bool(w)
            ok += any(x[1] for x in w)
        row.append("%14s" % ("%d/%d (%d)" % (ok, v, anyv) if v else "n/a"))
    print("%-6s" % e + "".join(row))
print("(valid/clean emits (any verdict))")
