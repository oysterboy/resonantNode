"""Per-group statistics of the SEQ_INSPECT amp/contrast values in bench logs.

Only occurrences that reached inspection carry values (SEQ DIAG on, verbose=1,
mode=detail). Groups are whole files, or sections of one long OBS run when a
section list is given.

Section list (text file, one section per line, '#' comments allowed):
    START  END  TITLE        (START/END as seconds or m:ss, from the audio file start)
The OBS window of trial N starts at (N-1)*period + offset seconds into the audio
(--offset-s = delay between audio start and the SEQ OBS start, default 0).

Examples:
  python tools/bench/inspectstats.py bench/sessions/2026-10-15-ampdiag-110cm/*.log
  python tools/bench/inspectstats.py run.log --sections sections.txt --offset-s 3 \
      --contrast-threshold 30500 --amp-threshold 2500
"""
import argparse
import re
import statistics
import sys

ap = argparse.ArgumentParser()
ap.add_argument("logs", nargs="+")
ap.add_argument("--sections", help="section list for a long OBS run (single log)")
ap.add_argument("--offset-s", type=float, default=0.0)
ap.add_argument("--period-ms", type=int, help="OBS period (default: parsed from the log, else 2000)")
ap.add_argument("--contrast-threshold", type=float, action="append", default=[],
                help="report how many occurrences have contrast P75 >= value (repeatable)")
ap.add_argument("--amp-threshold", type=float, action="append", default=[],
                help="report how many occurrences have amp P75 >= value (repeatable)")
args = ap.parse_args()


def secs(text):
    if ":" in text:
        m, s = text.split(":", 1)
        return int(m) * 60 + float(s)
    return float(text)


def load_sections(path):
    out = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            parts = line.split(None, 2)
            out.append((secs(parts[0]), secs(parts[1]), parts[2] if len(parts) > 2 else "%s-%s" % (parts[0], parts[1])))
    return out


def kv(line):
    return dict(re.findall(r"(\S+?)=(\S+)", line))


def parse(path):
    """Return (trial_count, period_ms, [(trial, label->fields)])."""
    period = args.period_ms
    trials = 0
    cur = None
    occ = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if period is None:
                m = re.search(r"SEQ OBS start .*?period=(\d+)", line)
                if m:
                    period = int(m.group(1))
            if line.startswith("SEQ_TRIAL "):
                cur = int(kv(line)["trial"])
                trials = max(trials, cur)
            elif line.startswith("SEQ_INSPECT ") and cur is not None:
                d = kv(line)
                if d.get("inspect.label") in ("amp", "contrast") and d.get("inspect.valid") == "1":
                    occ.append((cur, d["inspect.label"], d))
    return trials, period or 2000, occ


def collect(occ):
    by = {}
    for trial, label, d in occ:
        by.setdefault(trial, {})[label] = d
    return by


def rng(vals):
    if not vals:
        return "-"
    return "%.0f-%.0f (med %.0f)" % (min(vals), max(vals), statistics.median(vals))


def report(name, windows, by):
    amp = [float(v["amp"]["inspect.p75"]) for v in by.values() if "amp" in v]
    amp90 = [float(v["amp"]["inspect.p90"]) for v in by.values() if "amp" in v]
    amppk = [float(v["amp"]["inspect.peak"]) for v in by.values() if "amp" in v]
    con = [float(v["contrast"]["inspect.p75"]) for v in by.values() if "contrast" in v]
    print("%-34s windows %3d  inspected %3d" % (name, windows, len(by)))
    print("    amp P75      %s" % rng(amp))
    print("    amp P90      %s" % rng(amp90))
    print("    amp peak     %s" % rng(amppk))
    print("    contrast P75 %s" % rng(con))
    for t in args.contrast_threshold:
        print("    contrast P75 >= %.0f: %d of %d" % (t, sum(1 for c in con if c >= t), len(con)))
    for t in args.amp_threshold:
        print("    amp P75 >= %.0f: %d of %d" % (t, sum(1 for a in amp if a >= t), len(amp)))


if args.sections:
    if len(args.logs) != 1:
        sys.exit("--sections takes exactly one log")
    trials, period, occ = parse(args.logs[0])
    by = collect(occ)
    print("%s: %d windows, period %d ms, offset %.1f s" % (args.logs[0], trials, period, args.offset_s))
    for start, end, title in load_sections(args.sections):
        sel = {t: v for t, v in by.items() if start <= (t - 1) * period / 1000.0 + args.offset_s < end}
        n = sum(1 for t in range(1, trials + 1) if start <= (t - 1) * period / 1000.0 + args.offset_s < end)
        report(title, n, sel)
else:
    for path in args.logs:
        if path.endswith(("console.log", "README.md", "session.json")):
            continue
        trials, period, occ = parse(path)
        report(path.replace("\\", "/").split("/")[-1], trials, collect(occ))
