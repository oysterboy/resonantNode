"""Evaluate a deskrun.py session per section: Whispernet's passing phrases and Behavior's decisions, the phrases'
lengths, and the WHISPER status counters' change over the section (asked, answered, replays, deaf time, cut
phrases, the canceller's ERLE); for Nodes the emits and valid verdicts.

  python tools/bench/deskeval.py DIR
"""
import glob
import json
import os
import re
import sys
from collections import Counter, defaultdict

d = sys.argv[1]
schedule = json.load(open(os.path.join(d, "schedule.json"), encoding="utf-8"))
KV = re.compile(r"(\w+)=(\S+)")
COUNTERS = ("phrases", "asked", "answered", "replays", "deafs", "deaf_ms", "cuts")


def section_of(t):
    for i, s in enumerate(schedule):
        if s["start"] <= t < s["end"] + 2.0:  # a phrase may end, a status arrive just after
            return i
    return -1


for log in sorted(glob.glob(os.path.join(d, "E*.log"))):
    board = os.path.splitext(os.path.basename(log))[0]
    heard = defaultdict(Counter)
    durs = defaultdict(list)
    status = defaultdict(list)
    node = defaultdict(Counter)
    for line in open(log, encoding="utf-8", errors="replace"):
        ts, _, text = line.rstrip("\n").partition(" ")
        try:
            t = float(ts)
        except ValueError:
            continue
        sec = section_of(t)
        if text.startswith("WHISPER heard"):
            m = re.search(r"decision=(\w+)", text)
            heard[sec][m.group(1) if m else "?"] += 1
        elif text.startswith("WHISPER phrase"):
            kv = dict(KV.findall(text))
            durs[sec].append(int(kv.get("dur_ms", 0)))
        elif text.startswith("WHISPER status"):
            status[sec].append(dict(KV.findall(text)))
        elif text.startswith("RB emit accepted"):
            m = re.search(r"source=(\w+)", text)
            node[sec]["emit_" + (m.group(1) if m else "?")] += 1
        elif text.startswith("RB verdict=") and "valid=1" in text:
            node[sec]["valid"] += 1
    print("## %s" % board)
    prev = None
    for name, s in enumerate(schedule):
        line = "  %-24s" % s["name"]
        if heard[name]:
            line += " heard %s" % dict(heard[name])
        if durs[name]:
            ds = sorted(durs[name])
            line += "  phrases %d, ms median %d max %d" % (len(ds), ds[len(ds) // 2], ds[-1])
        if status[name]:
            last = status[name][-1]
            if prev:
                delta = {k: int(last.get(k, 0)) - int(prev.get(k, 0)) for k in COUNTERS if k in last}
                line += "  delta %s" % delta
            line += "  erle %s" % last.get("aec_erle_db", "-")
            prev = last
        if node[name]:
            line += "  node %s" % dict(node[name])
        print(line)
