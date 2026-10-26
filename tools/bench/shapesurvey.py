"""DET-015 verification (E002): shape facts per verdict over the recorded corpus, grouped by what was played. Usage: python tools/bench/shapesurvey.py <native-replay program> (host compiler runtime on PATH)."""
import glob
import os
import re
import subprocess
import sys
from collections import defaultdict

PROG = sys.argv[1]
dirs = ["test/fixtures"] + sorted(d for d in glob.glob("bench/sessions/*") if os.path.isdir(d) and (
    "corpus" in d or "cell" in d or "dur40" in d or "freqcols" in d))


def category(name, d):
    n = name.lower()
    if "noise" in d or "radio" in n or "music" in n or "speech" in n or "ambient" in n or "silence" in n:
        return "noise/speech/music"
    if "dur40" in d or "beep" in n or "dur40" in n:
        return "beep40"
    if "hop" in n:
        if "height25" in d:
            return "hop (height +25 cm)"
        if "dry" in n or "wet" in n or "notch" in n:
            return "hop (variants: dry/wet gaps)"
        return "hop"
    if "chirp" in n or "tone" in n or re.search(r"\d{4}_", n):
        return "chirp / steady tone"
    return "other:" + n[:20]


SHAPE_KEYS = ("start_ms", "dur_ms", "active_ms", "runs", "distinct", "up_steps", "other_steps", "seq")


def shape_fields(line):
    """A replay `shape <label> k=v ...` line as (label, start_ms, ..., seq), or None. Reads k=v pairs, so a field
    the replay adds (dips_pct, edips_pct came with the tool) does not make every line drop silently (critique
    D4.1); a line that lacks one of SHAPE_KEYS stops the survey instead of being skipped."""
    m = re.match(r"\s+shape (\S+) (.*)", line)
    if not m:
        return None
    f = dict(re.findall(r"(\w+)=(\S*)", m.group(2)))
    missing = [k for k in SHAPE_KEYS if k not in f]
    if missing:
        sys.exit("shapesurvey: shape line without %s: %s" % (",".join(missing), line.strip()))
    return (m.group(1),) + tuple(f[k] for k in SHAPE_KEYS)


rows = []
for d in dirs:
    out = subprocess.run([PROG, "--shape", "--dir", d], capture_output=True, text=True).stdout.splitlines()
    pending = []
    for line in out:
        g = shape_fields(line)
        if g:
            pending.append(g)
            continue
        m = re.match(r"(\S+\.(?:csv|wav))\s", line)
        if m:
            for g in pending:
                rows.append((d, m.group(1), category(m.group(1), d)) + g)
            pending = []

by = defaultdict(list)
for r in rows:
    by[r[2]].append(r)
for cat in sorted(by):
    rs = by[cat]
    print("== %s: %d verdicts (%d accepted today)" % (cat, len(rs), sum(1 for r in rs if r[3] == "accepted")))
    dist = defaultdict(int)
    for r in rs:
        dist[(int(r[8]), int(r[9]))] += 1
    print("   (distinct bins, up steps) -> count:", dict(sorted(dist.items())))
    for r in rs[:3]:
        print("   e.g. %-34s %-22s dur=%s seq=%s" % (r[1][:34], r[3], r[5], r[11][:90]))
