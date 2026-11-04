"""Evaluate a long nodegroup free run: emissions by source, verdicts and decisions, chains, bases over time,
port drops / reopens, resets, dropped buffers, per 6 h block."""
import re
import sys
from collections import Counter, defaultdict

LOW, BIN = 500, 250
path = sys.argv[1]
emits, verdicts, decisions, status, events, builds = [], Counter(), Counter(), [], [], []
src_by = defaultdict(Counter)
last_t = 0.0
for line in open(path, encoding="utf-8", errors="replace"):
    if line.startswith("#"):
        continue
    p = line.rstrip("\n").split(" ", 2)
    if len(p) < 3:
        continue
    try:
        t = float(p[0])
    except ValueError:
        continue
    last_t = max(last_t, t)
    n, text = p[1], p[2]
    if text.startswith("RB emit accepted"):
        m = re.search(r"source=(\w+)", text)
        src_by[n][m.group(1) if m else "?"] += 1
    m = re.match(r"RB emit shape=hop .*?order=(\d+)", text)
    if m:
        emits.append((t, n, (int(m.group(1)) - LOW) // BIN))
    if text.startswith("RB verdict=") and "valid=1" in text:
        verdicts[n] += 1
        m = re.search(r"decision=(\w+)", text)
        decisions[m.group(1) if m else "?"] += 1
    if text.startswith("# port") or "port lost" in text or "port reopened" in text or "setup sent again" in text:
        events.append((t, n, text[:80]))
    if "BUILD role=" in text or "rst:" in text:
        builds.append((t, n, text[:60]))
    if text.startswith("RB STATUS"):
        kv = dict(x.split("=", 1) for x in text.split() if "=" in x)
        status.append((t, n, kv))
emits.sort()
H = last_t / 3600
print("log span %.1f h" % H)
print("emissions by board and source:")
for n in sorted(src_by):
    print("  %s %s" % (n, dict(src_by[n])))
tot_idle = sum(c["idle"] for c in src_by.values())
tot_ans = sum(c.get("heard_pattern", 0) for c in src_by.values())
print("  total idle %d, answers %d, per hour %.0f / %.0f" % (tot_idle, tot_ans, tot_idle / H, tot_ans / H))
print("valid verdicts by board:", dict(verdicts))
print("decisions on valid verdicts:", dict(decisions.most_common(8)))
# chains: hop emissions separated by > 3 s
chains, cur = [], []
for e in emits:
    if cur and e[0] - cur[-1][0] > 3.0:
        chains.append(cur)
        cur = []
    cur.append(e)
if cur:
    chains.append(cur)
lens = [len(c) for c in chains]
durs = [c[-1][0] - c[0][0] for c in chains]
print("chains (3 s gap): %d; length median %d, max %d; duration max %.1f s; >30 s: %d; length>=6: %d" % (
    len(chains), sorted(lens)[len(lens) // 2], max(lens), max(durs), sum(1 for d in durs if d > 30),
    sum(1 for l in lens if l >= 6)))
# per 6 h block: emissions/h and the bases used
print("per 6 h block: emissions/h, chains, bases played (position: count)")
for b in range(int(H // 6) + 1):
    es = [e for e in emits if b * 21600 <= e[0] < (b + 1) * 21600]
    cs = [c for c in chains if b * 21600 <= c[0][0] < (b + 1) * 21600]
    span = min(21600, last_t - b * 21600) / 3600
    if span <= 0:
        continue
    print("  %2d-%2d h  %5.0f/h  %4d chains  %s" % (b * 6, b * 6 + 6, len(es) / span, len(cs),
                                                 dict(sorted(Counter(e[2] for e in es).items()))))
# pairs within reach at STATUS rounds (|base diff| <= 2)
rounds = defaultdict(dict)
for t, n, kv in status:
    if "behavior.base_bin" in kv:
        rounds[round(t / 600)][n] = int(kv["behavior.base_bin"])
reach = []
for r, bases in rounds.items():
    ns = sorted(bases)
    pairs = [(a, b) for i, a in enumerate(ns) for b in ns[i + 1:]]
    if pairs:
        reach.append(sum(1 for a, b in pairs if abs(bases[a] - bases[b]) <= 2) / len(pairs))
if reach:
    print("pairs within 2 positions at STATUS rounds: median %.0f %%, min %.0f %%" % (
        100 * sorted(reach)[len(reach) // 2], 100 * min(reach)))
# board health from the last STATUS
for n in sorted({s[1] for s in status}):
    kv = [s for s in status if s[1] == n][-1][2]
    print("  %s last STATUS: uptime_ms=%s dropped=%s heap_min=%s base=%s" % (
        n, kv.get("uptime_ms", kv.get("uptime_s", "?")), kv.get("audio.dropped_dma_buffers", kv.get("dropped", "?")),
        kv.get("heap.min_free", "?"), kv.get("behavior.base_bin", "?")))
print("port events: %d" % len(events))
for e in events[:20]:
    print("  %.0f s %s %s" % e)
print("boots / resets seen: %d" % len(builds))
for b in builds[:10]:
    print("  %.0f s %s %s" % b)
