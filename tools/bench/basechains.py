"""(BEH-009, step 17.3) Base bins in a nodegroup log: per emission the base (the train's first bin), chains split by a 3 s silence; for
chains of six or more, distinct bases among the first three and the last three, and "settled" (the last three on
one base). Also each node's last base, glides and scatters from RB STATUS.

  python tools/bench/basechains.py bench/sessions/<folder>/<nodegroup log>
"""
import re
import sys
from collections import Counter

LOW, BIN = 2950, 250
emits = []
status = []
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    p = line.split(" ", 2)
    if len(p) < 3:
        continue
    try:
        t = float(p[0])
    except ValueError:
        continue
    m = re.search(r"RB emit shape=hop .*?order=(\d+)", p[2])
    if m:
        emits.append((t, p[1], (int(m.group(1)) - LOW) // BIN))
    m = re.search(r"behavior\.base_bin=(\d+).*?behavior\.base_glides=(\d+).*?behavior\.base_scatters=(\d+)", p[2])
    if m and p[2].startswith("RB STATUS"):
        status.append((t, p[1], int(m.group(1)), int(m.group(2)), int(m.group(3))))
emits.sort()
chains = []
for e in emits:
    if not chains or e[0] - chains[-1][-1][0] >= 3.0:
        chains.append([e])
    else:
        chains[-1].append(e)
longc = [c for c in chains if len(c) >= 6]
df = sum(len({b for _, _, b in c[:3]}) for c in longc)
dl = sum(len({b for _, _, b in c[-3:]}) for c in longc)
st = sum(1 for c in longc if len({b for _, _, b in c[-3:]}) == 1)
print("emissions %d (hop trains), chains %d, of 6+ trains %d" % (len(emits), len(chains), len(longc)))
if longc:
    print("distinct bases first 3 %.2f -> last 3 %.2f, settled %.0f %%" % (df / len(longc), dl / len(longc),
                                                                        100.0 * st / len(longc)))
print("bases played overall:", dict(sorted(Counter(b for _, _, b in emits).items())))
last = {}
for t, n, b, g, s in status:
    last[n] = (b, g, s)
print("last STATUS per node (base, glides, scatters):", dict(sorted(last.items())))
