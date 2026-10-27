"""Count Node emissions that overlap another node's train, and whether listeners heard them (lab E003).

Reads a nodegroup / soakcycle log offline (host time in column 1, 'Ex BEH emit start' and 'Ex RB verdict ...
valid=N' lines). A round is a run of emission starts each within --train-s of the previous one, so a round with
two or more starts had trains sounding at once. For each round, every Node that did not emit within
[start - 0.5 s, end + 0.1 s] is a listener cell; it counts as heard when that Node logs a valid=1 verdict in
[start + 0.08 s, end + 0.3 s], invalid-only when it logs only valid=0 verdicts there. Rounds of two are split by
whether the two rotations are adjacent (neighbouring band bins at four of five steps). Rotations come from the
log's '# state ... output.hop_rotation' lines, else the roster (E1=0 ... E5=4).

Caveat: resting or refractory Nodes are not excluded as listeners, so the single-train rate is a floor, not a
like-for-like comparison with the rounds of two or more.

  python tools/bench/overlapstats.py bench/sessions/2026-10-22-step10b-field-rerun/2_free_run_quiet_30min.log
"""
import argparse
import bisect
import collections
import re

ap = argparse.ArgumentParser()
ap.add_argument("logs", nargs="+")
ap.add_argument("--train-s", type=float, default=0.165, help="hop train length (5 x 25 ms + 4 x 10 ms)")
ap.add_argument("--bins", type=int, default=5)
a = ap.parse_args()

EMIT = re.compile(r"^([\d.]+) (E\d) BEH emit start .*source=(\w+)")
VERDICT = re.compile(r"^([\d.]+) (E\d) RB verdict.* valid=(\d)")
ROT = re.compile(r"^# state (E\d) << PARAM path=output\.hop_rotation .*value=(\d+)")

for path in a.logs:
    emits, verdicts = [], []
    rot = {f"E{i + 1}": i for i in range(5)}
    with open(path, errors="replace") as f:
        for line in f:
            m = EMIT.match(line)
            if m:
                emits.append((float(m.group(1)), m.group(2)))
                continue
            m = VERDICT.match(line)
            if m:
                verdicts.append((float(m.group(1)), m.group(2), int(m.group(3))))
                continue
            m = ROT.match(line)
            if m:
                rot[m.group(1)] = int(m.group(2))
    if not emits:
        print(f"{path}: no emissions")
        continue
    emits.sort()
    verdicts.sort()
    et = [e[0] for e in emits]
    vt = [v[0] for v in verdicts]
    nodes = sorted({n for _, n in emits})

    rounds, cur = [], [emits[0]]
    for e in emits[1:]:
        if e[0] - cur[-1][0] <= a.train_s:
            cur.append(e)
        else:
            rounds.append(cur)
            cur = [e]
    rounds.append(cur)

    sizes = collections.Counter(len(r) for r in rounds)
    spreads = sorted(r[-1][0] - r[0][0] for r in rounds if len(r) > 1)
    cells = collections.defaultdict(lambda: [0, 0, 0])
    for r in rounds:
        t0, t1 = r[0][0], r[-1][0] + a.train_s
        busy = set()
        i = bisect.bisect_left(et, t0 - 0.5)
        while i < len(emits) and emits[i][0] <= t1 + 0.1:
            busy.add(emits[i][1])
            i += 1
        if len(r) == 2:
            d = (rot.get(r[0][1], 0) - rot.get(r[1][1], 0)) % a.bins
            key = "2 adjacent" if d in (1, a.bins - 1) else "2 non-adjacent"
        else:
            key = str(len(r))
        for n in nodes:
            if n in busy:
                continue
            hit = invalid = False
            j = bisect.bisect_left(vt, t0 + 0.08)
            while j < len(verdicts) and verdicts[j][0] <= t1 + 0.3:
                if verdicts[j][1] == n:
                    hit |= verdicts[j][2] == 1
                    invalid |= verdicts[j][2] == 0
                j += 1
            c = cells[key]
            c[0] += 1
            c[1] += hit
            c[2] += invalid and not hit
    multi = sum(v for k, v in sizes.items() if k > 1)
    print(f"{path}")
    print(f"  nodes {','.join(nodes)}  emissions {len(emits)}  rounds {len(rounds)}  "
          f"rounds with 2+ trains {multi}  round sizes {dict(sorted(sizes.items()))}")
    if spreads:
        print(f"  start spread in rounds of 2+: median {spreads[len(spreads) // 2] * 1000:.0f} ms, "
              f"max {spreads[-1] * 1000:.0f} ms")
    for key in sorted(cells):
        t, h, inv = cells[key]
        print(f"  trains at once {key:15s} listener cells {t:6d}  valid {h:6d} ({100 * h / t:3.0f} %)  "
              f"invalid only {inv:5d} ({100 * inv / t:3.0f} %)")
