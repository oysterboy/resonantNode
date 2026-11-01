"""Evaluate a wsession.py run: per phase, the boards' phrases (level, modulation, flatness) and the recordings'
flatness by the board's method; a proposed flat= bound between the whispers and the rustling.

  wseval.py DIR
"""
import base64
import glob
import json
import os
import re
import sys
import wave

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bandflat import block_flatness  # noqa: E402  (the board's flatness, modelled)

d = sys.argv[1]
schedule = json.load(open(os.path.join(d, "schedule.json")))


def phase_of(t):
    for p in schedule:
        if p["start"] - 1.0 <= t < p["end"] + 1.5:  # a phrase may close just after its phase
            return p["phase"]
    return "outside"


phr, rec = {}, {}
os.makedirs(os.path.join(d, "rec"), exist_ok=True)
for log in sorted(glob.glob(os.path.join(d, "E*.log"))):
    board = os.path.splitext(os.path.basename(log))[0]
    head, data, t_head = None, [], 0.0
    for line in open(log, encoding="utf-8", errors="replace"):
        ts, _, text = line.rstrip("\n").partition(" ")
        t = float(ts)
        if text.startswith("WHISPER phrase"):
            kv = dict(x.split("=", 1) for x in text.split()[2:] if "=" in x)
            phr.setdefault(phase_of(t), []).append((board, kv))
        elif text.startswith("WHISPER_REC_BEGIN"):
            head, data, t_head = text, [], t
        elif text.startswith("WHISPER_REC ") and head:
            data.append(text[len("WHISPER_REC "):].strip())
        elif text.startswith("WHISPER_REC_END") and head:
            raw = b"".join(base64.b64decode(x) for x in data)
            seq = re.search(r"seq=(\d+)", head).group(1)
            ph = phase_of(t_head - 1.2)  # the dump comes 1.2 s after the check
            name = os.path.join(d, "rec", "%s_%s_rec%s.wav" % (ph, board, seq))
            with wave.open(name, "wb") as w:
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(24000)
                w.writeframes(raw)
            x = np.frombuffer(raw, "<i2").astype(float)
            f = block_flatness(x)
            rec.setdefault(ph, []).append(float(np.median(f)) if f else float("nan"))
            head = None

print("phase            phrases  flat (median p10..p90 of phrases)   level_p90 med   mod med   recordings' flatness")
summary = {}
for p in [s["phase"] for s in schedule if s["phase"] != "pause"] + ["pause", "outside"]:
    rows = phr.get(p, [])
    flats = [float(kv["flat"]) for _, kv in rows if kv.get("flat", "na") != "na"]
    lv = [float(kv["level_p90"]) for _, kv in rows]
    md = [float(kv["mod"]) for _, kv in rows]
    rf = [v for v in rec.get(p, []) if v == v]
    summary[p] = flats
    print("%-16s %5d   %s   %8s   %6s   %s" % (
        p, len(rows),
        "%.2f %.2f..%.2f" % (np.median(flats), np.percentile(flats, 10), np.percentile(flats, 90)) if flats else "-",
        "%.0f" % np.median(lv) if lv else "-", "%.2f" % np.median(md) if md else "-",
        " ".join("%.2f" % v for v in sorted(rf)) if rf else "-"))
whisper = [v for k, v in summary.items() if k.startswith("whisper") for v in v]
rustle = summary.get("rustle_typing", [])
if whisper and rustle:
    hi, lo = np.percentile(whisper, 90), np.percentile(rustle, 10)
    print("\nwhispers p90 %.2f, rustling p10 %.2f -> flat= %.2f%s" % (hi, lo, (hi + lo) / 2,
                                                                   "" if hi < lo else "  (OVERLAP: no clean bound)"))
