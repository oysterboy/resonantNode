"""Hearing test, the Analyzer over radio: one node plays, the other only listens, then the other way round; every call gets an outcome.

  python tools/bench/hearingtest.py --gateway E0=COM12 --nodes R1,E2 --calls bb-all,bb-trill,cricket,glide \
      --window 2500-3500 --block 300 --rounds 2 --out bench/sessions/<folder>

Why one-way: with both playing, the nodes answer each other, so one miss breaks a chain and the counts clump. Here the
listener is muted (output.silent 1, it still detects), so the player never hears an answer and calls on its silence
timer (behavior.call_after_silence_ms --every, spread --spread, set live): each call is one independent trial.

Order: per round every call in both directions, the calls' order reversed every other round (A B C D, D C B A), so a
slow drift of the room does not land on one call. Everything is live (nothing saved); at the end every node is muted
and the silence timer is back at 30000 / 10000.

Outcome per call (the player's "RB emit"), from the listener's lines in the --match seconds after it, best first:
  heard     a verdict valid=1
  invalid   a verdict valid=0 (reject= / failed= say which inspection failed)
  dismissed an "RB reject" line: the detector refused the candidate (reason=: too short / long, weak, coverage, ...)
  missed    nothing at all: no candidate opened, or it was dropped without a reject
An invalid call carries its RB inspect values (each check: found / needed : value), a dismissed one the detector's
measurements, a missed one the loudest input of the RB level lines in its window against the onset threshold.
Listener events outside every call's window are counted as stray. Needs a Node image with the RB reject / inspect /
level lines (notes_manual.md, "Hearing test"); an older image gives outcomes without reasons.

Writes block_<n>_<call>_<player>-<listener>.log, calls.csv (one row per call), summary.csv and changes.txt.
"""
import argparse
import collections
import csv
import math
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nodepreset  # noqa: E402

LOG = re.compile(r"NET log from=(\S+) (.*)")


def wilson(k, n, z=1.96):
    """95 % range of a rate k/n in percent (Wilson): honest at small n and near 0 or 100 %."""
    if n == 0:
        return None, None
    p = k / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d
    return round(100 * (c - h)), round(100 * (c + h))


def field(text, key):
    m = re.search(r"\b%s=(\S+)" % key, text)
    return m.group(1) if m else ""


def classify(path, player, listener, match):
    """Outcome per player call from one block's log."""
    emits, events, levels = [], [], []
    for line in open(path, encoding="utf-8"):
        p = line.rstrip("\n").split(" ", 2)
        if len(p) < 3:
            continue
        t, who, text = float(p[0]), p[1], p[2]
        if who == player and text.startswith("RB emit shape="):
            emits.append(t)
        elif who == listener and (text.startswith("RB verdict=") or text.startswith("RB reject ")):
            events.append((t, text))
        elif who == listener and text.startswith("RB inspect ") and events and events[-1][1].startswith("RB verdict="):
            events[-1] = (events[-1][0], events[-1][1] + " || " + text)
        elif who == listener and text.startswith("RB level "):
            levels.append((t, text))
    rows, used = [], set()
    rank = {"heard": 0, "invalid": 1, "dismissed": 2}
    for i, t in enumerate(emits):
        end = min(t + match, emits[i + 1] if i + 1 < len(emits) else t + match)
        best, detail = "missed", ""
        for j, (te, text) in enumerate(events):
            if te < t or te >= end:
                continue
            used.add(j)
            if text.startswith("RB verdict="):
                kind = "heard" if "valid=1" in text else "invalid"
                why = "" if kind == "heard" else "%s/%s" % (field(text, "reject"), field(text, "failed"))
                why += " dur=%s" % field(text, "duration_ms")
                if kind == "invalid" and " || RB inspect " in text:
                    why += " [%s]" % text.split(" || RB inspect ", 1)[1]
            else:
                kind, why = "dismissed", "%s dur=%s strength=%s coverage=%s island=%s gap=%s" % (
                    field(text, "reason"), field(text, "duration_ms"), field(text, "strength"),
                    field(text, "coverage_ms"), field(text, "island_ms"), field(text, "gap_ms"))
            if best == "missed" or rank[kind] < rank[best]:
                best, detail = kind, why
        if best == "missed":
            near = [x for tl, x in levels if t <= tl < t + match + 1.0]
            if near:
                peak = max(int(field(x, "peak") or 0) for x in near)
                onset = field(near[0], "onset")
                detail = "%s peak=%d onset=%s" % ("quiet" if onset and peak < int(onset) else "loud", peak, onset)
        rows.append((t, best, detail.strip()))
    stray = len(events) - len(used)
    return rows, stray


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gateway", required=True)
    ap.add_argument("--nodes", required=True, help="two nodes, A,B: A plays to B, then B to A")
    ap.add_argument("--calls", default="glide,cricket")
    ap.add_argument("--window", required=True, help="low-high Hz, one window for both")
    ap.add_argument("--block", type=int, default=300, help="seconds per call and direction")
    ap.add_argument("--rounds", type=int, default=2)
    ap.add_argument("--every", type=int, default=2500, help="the player's call_after_silence_ms")
    ap.add_argument("--spread", type=int, default=1000)
    ap.add_argument("--match", type=float, default=1.5, help="seconds after a call that count for it")
    ap.add_argument("--settle", type=int, default=8)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    gw = nodepreset.parse(a.gateway)[0]
    x, y = [n for n in a.nodes.split(",") if n][:2]
    calls = [c for c in a.calls.split(",") if c]
    lo, hi = a.window.split("-")

    def say(text):
        line = "%s %s" % (time.strftime("%H:%M:%S"), text)
        print(line, flush=True)
        with open(os.path.join(a.out, "changes.txt"), "a", encoding="utf-8") as f:
            f.write(line + "\n")

    def send(cmds, targets):
        nodepreset.apply_radio(gw, targets, cmds)

    tally = collections.defaultdict(collections.Counter)
    reasons = collections.defaultdict(collections.Counter)
    allrows = []
    block = 0
    for r in range(a.rounds):
        order = calls if r % 2 == 0 else calls[::-1]
        for call in order:
            for player, listener in ((x, y), (y, x)):
                block += 1
                _, cmds = nodepreset.read_preset(call)
                say("block %d round %d: %s, %s plays to %s, window %s-%s Hz" % (block, r + 1, call, player, listener,
                                                                               lo, hi))
                send(cmds + ["PARAM SET behavior.band_per_node 1", "PARAM SET behavior.band_range_low_hz %s" % lo,
                             "PARAM SET behavior.band_range_high_hz %s" % hi, "PARAM SET node.radio_log 20",
                             "RB log full"], [x, y])
                send(["PARAM SET output.silent 1"], [listener])
                send(["PARAM SET behavior.call_enabled 1", "PARAM SET behavior.call_after_silence_ms %d" % a.every,
                      "PARAM SET behavior.call_after_silence_spread_ms %d" % a.spread, "PARAM SET output.silent 0"],
                     [player])
                time.sleep(a.settle)
                path = os.path.join(a.out, "block_%02d_%s_%s-%s.log" % (block, call, player, listener))
                s = nodepreset.open_port(gw[1], timeout=0.05)
                t0, buf = time.time(), b""
                with open(path, "w", encoding="utf-8") as f:
                    while time.time() - t0 < a.block:
                        chunk = s.read(4096)
                        if not chunk:
                            continue
                        buf += chunk
                        while b"\n" in buf:
                            raw, buf = buf.split(b"\n", 1)
                            m = LOG.match(raw.decode("ascii", errors="replace").strip())
                            if m:
                                f.write("%.2f %s %s\n" % (time.time() - t0, m.group(1), m.group(2)))
                s.close()
                send(["PARAM SET output.silent 1"], [player])
                rows, stray = classify(path, player, listener, a.match)
                key = (call, "%s->%s" % (player, listener))
                c = collections.Counter(o for _, o, _ in rows)
                tally[key].update(c)
                tally[key]["stray"] += stray
                for _, o, d in rows:
                    if o in ("invalid", "dismissed", "missed") and d:
                        reasons[key][o + " " + d.split(" ")[0]] += 1
                for t, o, d in rows:
                    allrows.append([block, r + 1, call, player, listener, "%.2f" % t, o, d])
                say("  calls=%d heard=%d invalid=%d dismissed=%d missed=%d stray=%d"
                    % (len(rows), c["heard"], c["invalid"], c["dismissed"], c["missed"], stray))
                with open(os.path.join(a.out, "calls.csv"), "w", newline="", encoding="utf-8") as f:
                    w = csv.writer(f)
                    w.writerow(["block", "round", "call", "player", "listener", "t", "outcome", "detail"])
                    w.writerows(allrows)
                write_summary(a.out, tally, reasons)
    send(["PARAM SET output.silent 1", "PARAM SET behavior.call_after_silence_ms 30000",
          "PARAM SET behavior.call_after_silence_spread_ms 10000"], [x, y])
    say("done: both muted, silence timer back at 30000 / 10000 (live)")
    for line in open(os.path.join(a.out, "summary.csv"), encoding="utf-8"):
        print(line.rstrip())


def write_summary(out, tally, reasons):
    with open(os.path.join(out, "summary.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["call", "direction", "calls", "heard", "heard_pct", "range95", "invalid", "dismissed", "missed",
                    "stray", "reasons"])
        for (call, d), c in sorted(tally.items()):
            n = c["heard"] + c["invalid"] + c["dismissed"] + c["missed"]
            lo, hi = wilson(c["heard"], n)
            top = "; ".join("%s x%d" % (k, v) for k, v in reasons[(call, d)].most_common(4))
            w.writerow([call, d, n, c["heard"], round(100 * c["heard"] / n) if n else "", "%s-%s" % (lo, hi),
                        c["invalid"], c["dismissed"], c["missed"], c["stray"], top])


if __name__ == "__main__":
    main()
