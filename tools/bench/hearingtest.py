"""Hearing test, the Analyzer over radio: one node plays, the other only listens, then the other way round; every call gets an outcome.

  python tools/bench/hearingtest.py --gateway E0=COM12 --nodes R1,E2 --calls bb-all,bb-trill,cricket,glide \
      --window 2500-3500 --block 300 --rounds 2 --out bench/sessions/<folder>

Why one-way: with both playing, the nodes answer each other, so one miss breaks a chain and the counts clump. Here the
listener is muted (output.silent 1, it still detects), so the player never hears an answer and calls on its silence
timer (behavior.call_after_silence_ms --every, spread --spread, set live): each call is one independent trial.

Order: window by window (--window takes several, comma-separated, run in the order given); per window and round
every call in both directions, the calls' order reversed every other round (A B C D, D C B A), so a
slow drift of the room does not land on one call. Everything is live (nothing saved); at the end every node is muted
and the silence timer is back at 30000 / 10000.

Outcome per call (the player's "RB emit"), from the listener's lines in the --match seconds after it, best first:
  heard     a verdict valid=1
  detected  an RB inspect with no verdict before it: the node detected, the radio dropped the verdict line
  invalid   a verdict valid=0 (reject= / failed= say which inspection failed)
  dismissed an "RB reject" line: the detector refused the candidate (reason=: too short / long, weak, coverage, ...)
  missed    nothing at all: no candidate opened, or it was dropped without a reject
An invalid call carries its RB inspect values (each check: found / needed : value), a dismissed one the detector's
measurements, a missed one the loudest input of the RB level lines in its window against the onset threshold.
Listener events outside every call's window are counted as stray. A call whose window runs past the end of
the block's log is left out. Only the call itself counts: the low announce tone of an idle call is not a call.

  python tools/bench/hearingtest.py --gateway - --nodes A,B --window L-H --rescore --out <folder>
re-scores the block logs already in <folder> without touching a board. Needs a Node image with the RB reject / inspect /
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
        # An idle call logs two emits: the low announce tone (shape=idle_call ... then=hop), then the call itself.
        # The announce lies outside the detection window, so counting it would halve every rate.
        if who == player and text.startswith("RB emit shape=") and not text.startswith("RB emit shape=idle_call"):
            emits.append(t)
        elif who == listener and (text.startswith("RB verdict=") or text.startswith("RB reject ")):
            events.append((t, text))
        elif who == listener and text.startswith("RB inspect "):
            # The node prints inspect right after its verdict. An inspect with no verdict before it means the radio
            # dropped the verdict line: the node did detect, so it is not a miss (seen 2026-11-18, mostly R1).
            if events and events[-1][1].startswith("RB verdict=") and " || " not in events[-1][1]:
                events[-1] = (events[-1][0], events[-1][1] + " || " + text)
            else:
                events.append((t, text))
        elif who == listener and text.startswith("RB level "):
            levels.append((t, text))
    rows, used = [], set()
    rank = {"heard": 0, "detected": 1, "invalid": 2, "dismissed": 3}
    last = max([te for te, _ in events] + [tl for tl, _ in levels] + emits + [0.0])
    for i, t in enumerate(emits):
        if t + match > last:
            continue  # the block's log closed before an answer could arrive; neither heard nor missed
        end = min(t + match, emits[i + 1] if i + 1 < len(emits) else t + match)
        best, detail = "missed", ""
        for j, (te, text) in enumerate(events):
            if te < t or te >= end:
                continue
            used.add(j)
            if text.startswith("RB inspect "):
                kind, why = "detected", "verdict line lost [%s]" % text[len("RB inspect "):]
            elif text.startswith("RB verdict="):
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


def listen_cmds(listen, lo, hi):
    """Where the nodes listen. own: a band per node, the window as its range (the detector listens on as many bins as
    the call has steps, from the node's base: one bin for a trill alone). band: the detector on the whole window."""
    cmds = ["PARAM SET behavior.band_range_low_hz %s" % lo, "PARAM SET behavior.band_range_high_hz %s" % hi]
    if listen == "band":
        return cmds + ["PARAM SET behavior.band_per_node 0", "PARAM SET detection.band_low_hz %s" % lo,
                       "PARAM SET detection.band_high_hz %s" % hi]
    return cmds + ["PARAM SET behavior.band_per_node 1"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gateway", required=True)
    ap.add_argument("--nodes", required=True, help="two nodes, A,B: A plays to B, then B to A")
    ap.add_argument("--calls", default="glide,cricket")
    ap.add_argument("--window", required=True,
                    help="low-high Hz, one window for both; several comma-separated run one after the other")
    ap.add_argument("--block", type=int, default=300, help="seconds per call and direction")
    ap.add_argument("--rounds", type=int, default=2)
    ap.add_argument("--every", type=int, default=2500, help="the player's call_after_silence_ms")
    ap.add_argument("--spread", type=int, default=1000)
    ap.add_argument("--match", type=float, default=1.5, help="seconds after a call that count for it")
    ap.add_argument("--settle", type=int, default=8)
    ap.add_argument("--out", required=True)
    ap.add_argument("--listen", choices=["own", "band"], default="own",
                    help="own: each node listens on its own bins (behavior.band_per_node 1, as many bins as its call "
                    "has steps); band: every node listens on the whole window (band_per_node 0, detection.band_*)")
    ap.add_argument("--rescore", action="store_true", help="re-classify the block logs in --out, no boards touched")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    tally = collections.defaultdict(collections.Counter)
    reasons = collections.defaultdict(collections.Counter)
    allrows = []

    def score(block, rnd, call, window, player, listener, path):
        rows, stray = classify(path, player, listener, a.match)
        key = (window, call, "%s->%s" % (player, listener))
        c = collections.Counter(o for _, o, _ in rows)
        tally[key].update(c)
        tally[key]["stray"] += stray
        for _, o, d in rows:
            if o in ("detected", "invalid", "dismissed", "missed") and d:
                reasons[key][o + " " + d.split(" ")[0]] += 1
        for t, o, d in rows:
            allrows.append([block, rnd, window, call, player, listener, "%.2f" % t, o, d])
        with open(os.path.join(a.out, "calls.csv"), "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(["block", "round", "window", "call", "player", "listener", "t", "outcome", "detail"])
            w.writerows(allrows)
        write_summary(a.out, tally, reasons)
        return "  calls=%d heard=%d detected=%d invalid=%d dismissed=%d missed=%d stray=%d" % (
            len(rows), c["heard"], c["detected"], c["invalid"], c["dismissed"], c["missed"], stray)

    if a.rescore:
        logs = sorted(f for f in os.listdir(a.out) if f.startswith("block_") and f.endswith(".log"))
        per_round = 2 * len({f.split("_")[2] for f in logs})
        for f in logs:
            p = f[:-4].split("_")
            if len(p) == 6:  # block_<n>_r<round>_<call>_<low-high>_<player-listener>
                n, rnd, call, window, pair = int(p[1]), int(p[2][1:]), p[3], p[4], p[5]
            else:  # the one-window name of 2026-11-18: block_<n>_<call>_<player-listener>
                n, call, pair = int(p[1]), p[2], p[3]
                rnd, window = (n - 1) // per_round + 1, a.window
            player, listener = pair.split("-")
            print(f, score(n, rnd, call, window, player, listener, os.path.join(a.out, f)))
        for line in open(os.path.join(a.out, "summary.csv"), encoding="utf-8"):
            print(line.rstrip())
        return
    gw = nodepreset.parse(a.gateway)[0]
    x, y = [n for n in a.nodes.split(",") if n][:2]
    calls = [c for c in a.calls.split(",") if c]
    windows = [w for w in a.window.split(",") if w]

    def say(text):
        line = "%s %s" % (time.strftime("%H:%M:%S"), text)
        print(line, flush=True)
        with open(os.path.join(a.out, "changes.txt"), "a", encoding="utf-8") as f:
            f.write(line + "\n")

    def send(cmds, targets):
        nodepreset.apply_radio(gw, targets, cmds)

    block = 0
    rounds = [(w, r) for w in windows for r in range(a.rounds)]
    for window, r in rounds:
        lo, hi = window.split("-")
        order = calls if r % 2 == 0 else calls[::-1]
        for call in order:
            for player, listener in ((x, y), (y, x)):
                block += 1
                _, cmds = nodepreset.read_preset(call)
                say("block %d round %d: %s, %s plays to %s, window %s-%s Hz" % (block, r + 1, call, player, listener,
                                                                               lo, hi))
                send(cmds + listen_cmds(a.listen, lo, hi) + ["PARAM SET node.radio_log 20", "RB log full"], [x, y])
                send(["PARAM SET output.silent 1"], [listener])
                send(["PARAM SET behavior.call_enabled 1", "PARAM SET behavior.call_after_silence_ms %d" % a.every,
                      "PARAM SET behavior.call_after_silence_spread_ms %d" % a.spread, "PARAM SET output.silent 0"],
                     [player])
                time.sleep(a.settle)
                path = os.path.join(a.out, "block_%02d_r%d_%s_%s_%s-%s.log" % (block, r + 1, call, window, player,
                                                                              listener))
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
                say(score(block, r + 1, call, window, player, listener, path))
    send(["PARAM SET output.silent 1", "PARAM SET behavior.call_after_silence_ms 30000",
          "PARAM SET behavior.call_after_silence_spread_ms 10000"], [x, y])
    say("done: both muted, silence timer back at 30000 / 10000 (live)")
    for line in open(os.path.join(a.out, "summary.csv"), encoding="utf-8"):
        print(line.rstrip())


def write_summary(out, tally, reasons):
    with open(os.path.join(out, "summary.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["window", "call", "direction", "calls", "heard", "heard_pct", "range95", "detected", "invalid", "dismissed",
                    "missed", "stray", "reasons"])
        for (window, call, d), c in sorted(tally.items(), key=lambda kv: (int(kv[0][0].split("-")[0]), kv[0][1:])):
            n = c["heard"] + c["detected"] + c["invalid"] + c["dismissed"] + c["missed"]
            lo, hi = wilson(c["heard"], n)
            top = "; ".join("%s x%d" % (k, v) for k, v in reasons[(window, call, d)].most_common(4))
            w.writerow([window, call, d, n, c["heard"], round(100 * c["heard"] / n) if n else "", "%s-%s" % (lo, hi),
                        c["detected"], c["invalid"], c["dismissed"], c["missed"], c["stray"], top])


if __name__ == "__main__":
    main()
