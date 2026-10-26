"""Interim (or after-the-fact) summary of a running nodegroup.py log: the 10c gate counts so far.

nodegroup.py prints its summary only at the end; this reads the log it is writing (or a finished one) and reports:
emissions and valid verdicts per board and hour, chains (ended within 30 s), answers started by noise (a
heard_pattern emission with no other board's emission in the 2 s before), dropped buffers / read errors /
verdict-queue overflows, resets (audio.uptime_ms going down), deaf boards, and the per-hour median strength per
listener <- sender pair.

  python tools/bench/nodegroup_interim.py bench/sessions/<folder>/<name>.log [--bin 3600]
"""
import argparse
import re
from collections import defaultdict

ap = argparse.ArgumentParser()
ap.add_argument("log")
ap.add_argument("--bin", type=float, default=3600.0)
ap.add_argument("--chain-gap", type=float, default=3.0)
ap.add_argument("--quiet-within", type=float, default=30.0)
a = ap.parse_args()

emits = []          # (t, name, source)
verdicts = []       # (t, name, strength or None)
trains = []         # (t, name): hop train starts
status = defaultdict(list)
last_t = 0.0
# RB STATUS outgrew 1 KB (2026-10-25: ~1.7 KB with the radio and heap fields) and reaches the PC in two
# pieces; nodegroup.py before that date logged the second piece as its own line (no "RB STATUS" prefix).
# A STATUS line without its closing BUILD field is held and joined with the board's next line.
held_status = {}


def log_lines(path):
    for line in open(path, encoding="utf-8", errors="replace"):
        if line.startswith("#"):
            continue
        m = re.match(r"([\d.]+) (E\d) (.*)", line.rstrip("\n"))
        if not m:
            continue
        t, n, l = float(m.group(1)), m.group(2), m.group(3)
        if n in held_status:
            t0, head = held_status.pop(n)
            if not re.match(r"(RB |BEH |PARAM |NET |SELFTEST|BUILD )", l):
                yield t0, n, head + l
                continue
            yield t0, n, head
        if l.startswith("RB STATUS") and " BUILD role=" not in l:
            held_status[n] = (t, l)
            continue
        yield t, n, l
    for n, (t0, head) in held_status.items():
        yield t0, n, head


for t, n, l in log_lines(a.log):
    last_t = max(last_t, t)
    if l.startswith("BEH emit start"):
        s = re.search(r"source=(\w+)", l)
        emits.append((t, n, s.group(1) if s else "?"))
    elif l.startswith("RB emit shape=hop"):
        trains.append((t, n))  # printed when the train itself starts (an idle call's ~700 ms after emit start)
    elif l.startswith("RB verdict=") and "valid=1" in l:
        s = re.search(r" strength=(\d+)", l)
        verdicts.append((t, n, int(s.group(1)) if s else None))
    elif l.startswith("RB STATUS"):
        f = dict(re.findall(r"([\w.]+)=(-?[\d.]+)", l))
        status[n].append((t, f))

names = sorted(set(n for _, n, _ in emits) | set(status))
nbins = int(last_t // a.bin) + 1
print("elapsed %.2f h, %d emissions, %d valid verdicts" % (last_t / 3600.0, len(emits), len(verdicts)))

# per board
for n in names:
    e = [x for x in emits if x[1] == n]
    v = [x for x in verdicts if x[1] == n]
    per_h = [sum(1 for t, _, _ in v if b * a.bin <= t < (b + 1) * a.bin) for b in range(nbins)]
    print("  %s: emits %d (heard %d, idle %d), valid verdicts per bin: %s" % (
        n, len(e), sum(1 for x in e if x[2] == "heard_pattern"), sum(1 for x in e if x[2] == "idle"),
        " ".join(str(x) for x in per_h)))

# health
for n in names:
    rows = status[n]
    if not rows:
        continue
    resets = sum(1 for i in range(1, len(rows))
                 if float(rows[i][1].get("audio.uptime_ms", 0)) < float(rows[i - 1][1].get("audio.uptime_ms", 0)))
    last = rows[-1][1]
    print("  %s health: STATUS %d, dropped %s, read_errors %s, verdict_queue_overflows %s, resets %d, stack_free_min %s, uptime %.2f h" % (
        n, len(rows), last.get("i2s.dropped_dma_buffers"), last.get("i2s.read_errors"),
        last.get("detection.verdict_queue_overflows"), resets, last.get("loop.stack_free_min_bytes"),
        float(last.get("audio.uptime_ms", 0)) / 3.6e6))

# noise-started answers
noise = 0
heard = [x for x in emits if x[2] == "heard_pattern"]
for t, n, _ in heard:
    if not any(nn != n and 0 <= t - te <= 2.0 for te, nn, _ in emits if te <= t):
        noise += 1
print("  answers without another board's emission in the 2 s before (noise-started): %d of %d" % (noise, len(heard)))

# chains: from each idle call, until a --chain-gap silence
times = sorted(t for t, _, _ in emits)
starts = sorted(t for t, _, s in emits if s == "idle")
chains = []
end_prev = -1.0
for k in starts:
    if k <= end_prev:
        continue
    prev = k
    for t in [t for t in times if t >= k] + [last_t + a.chain_gap + 1]:
        if t - prev >= a.chain_gap:
            break
        prev = t
    chains.append(prev - k)
    end_prev = prev
done = [d for d in chains if d <= a.quiet_within]
if chains:
    print("  chains: %d, ended within %.0f s: %d (max %.1f s)" % (len(chains), a.quiet_within, len(done), max(chains)))
per_min = defaultdict(int)
for t in times:
    per_min[int(t // 60)] += 1
print("  max emissions per minute: %d" % (max(per_min.values()) if per_min else 0))

# drift per pair
pairs = defaultdict(list)
for t, n, st in verdicts:
    if st is None:
        continue
    # Only an unambiguous verdict: exactly one other board started an emission in the window. When several
    # boards answer one call, their trains overlap and the louder one is heard, not always the latest
    # (2026-10-23 block 2: E4 <- E2 flipped between E2's ~5,800 and E5's ~24,000).
    senders = {ne for te, ne in trains if ne != n and 0.05 <= t - te <= 0.8}
    if len(senders) == 1:
        pairs[(n, senders.pop())].append((t, st))
print("  median strength per listener <- sender and bin, unambiguous verdicts only (first -> last full bin):")
for (x, y) in sorted(pairs):
    cells = []
    for b in range(nbins):
        vals = sorted(st for t, st in pairs[(x, y)] if b * a.bin <= t < (b + 1) * a.bin)
        cells.append(vals[len(vals) // 2] if vals else None)
    shown = [c for c in cells[:-1] if c is not None]  # the last bin is partial
    change = "%+.0f %%" % (100.0 * (shown[-1] - shown[0]) / shown[0]) if len(shown) >= 2 else "n/a"
    print("    %s <- %s: %s  (%s)" % (x, y, " ".join(str(c) if c is not None else "-" for c in cells), change))
