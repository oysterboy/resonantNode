"""Shared parsing for bench session logs (stdlib only)."""
import csv
import json
import os
import re

KV = re.compile(r"(\S+?)=(\S*)")
BANNER = re.compile(r"^BUILD role=(\S+) git=(\S+) .*version=(\S+)")
LINE_TYPES = ("SEQ_TRIAL", "SEQ_SOURCE_CORE", "SEQ_SOURCE_SPEC", "SEQ_SOURCE",
              "SEQ_INSPECT", "SEQ_EXPLAIN", "SEQ_DETAIL", "SEQ_SUMMARY", "SEQ REPORT")

# SEQ_SUMMARY fields copied into summaries and the index, in this order.
SUMMARY_FIELDS = ("trials", "completed", "expected_trials", "early_trials", "late_trials",
                  "miss_trials", "duplicate_trials", "unexpected_trials", "rejected_trials",
                  "buffer_overrun_trials", "detector_accepted_trials", "detector_reject_trials",
                  "pattern_valid_trials", "pattern_rejected_trials", "avg_dt_ms",
                  "avg_strength", "avg_conf")

INDEX_COLUMNS = ("session", "run", "date", "firmware", "role", "profile", "detector",
                 "distance_cm", "purpose") + SUMMARY_FIELDS

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BENCH_ROOT = os.path.join(REPO_ROOT, "bench")

if not os.path.isfile(os.path.join(BENCH_ROOT, "README.md")):
    raise SystemExit(
        "bench/ is not checked out. Bench data lives on the orphan branch 'bench':\n"
        "  git fetch origin bench && git worktree add bench bench\n"
        "(run from the main checkout; see docs/decisions/2026-10-08-bench-data-in-repo.md)")


def line_type(line):
    for t in LINE_TYPES:
        if line.startswith(t + " ") or line == t:
            return t
    return None


def parse_banner(text):
    for line in text.splitlines():
        m = BANNER.match(line.strip())
        if m:
            return {"role": m.group(1), "git": m.group(2), "version": m.group(3)}
    return None


def parse_run_log(path):
    """Return trials, summary, report, field keys per line type, line counts."""
    trials, summary, report, keys, counts = [], {}, {}, {}, {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f.read().splitlines():
            t = line_type(line)
            if not t:
                continue
            counts[t] = counts.get(t, 0) + 1
            kv = dict(KV.findall(line))
            keys.setdefault(t, {})
            for k in kv:
                keys[t][k] = None
            if t == "SEQ_TRIAL":
                trials.append(kv)
            elif t == "SEQ_SUMMARY":
                summary = kv
            elif t == "SEQ REPORT":
                report = kv
    return {"trials": trials, "summary": summary, "report": report,
            "keys": {t: list(v) for t, v in keys.items()}, "counts": counts}


def summarize_run(path):
    parsed = parse_run_log(path)
    s = parsed["summary"]
    results = {}
    for t in parsed["trials"]:
        r = t.get("result", "?")
        results[r] = results.get(r, 0) + 1
    out = {
        "profile": s.get("profile"),
        "detector": s.get("detector"),
        "complete": bool(s),
        "trial_results": results,
        "line_counts": parsed["counts"],
    }
    for k in SUMMARY_FIELDS:
        if k in s:
            out[k] = s[k]
    return out


def load_session(session_dir):
    with open(os.path.join(session_dir, "session.json"), encoding="utf-8") as f:
        return json.load(f)


def save_session(session_dir, data):
    with open(os.path.join(session_dir, "session.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=2)
        f.write("\n")


def rebuild_index():
    """Regenerate bench/index.csv from every session.json. Never hand-edit the CSV."""
    rows = []
    sessions_root = os.path.join(BENCH_ROOT, "sessions")
    for name in sorted(os.listdir(sessions_root)):
        sdir = os.path.join(sessions_root, name)
        if not os.path.isfile(os.path.join(sdir, "session.json")):
            continue
        data = load_session(sdir)
        setup = data.get("setup", {})
        for run in data.get("runs", []):
            summ = run.get("summary", {})
            row = {
                "session": name,
                "run": run["name"],
                "date": run.get("started", "")[:10],
                "firmware": run.get("firmware", {}).get("git", ""),
                "role": run.get("firmware", {}).get("role", ""),
                "profile": summ.get("profile", ""),
                "detector": summ.get("detector", ""),
                "distance_cm": setup.get("distance_cm", ""),
                "purpose": data.get("purpose", ""),
            }
            for k in SUMMARY_FIELDS:
                row[k] = summ.get(k, "")
            rows.append(row)
    path = os.path.join(BENCH_ROOT, "index.csv")
    with open(path, "w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=INDEX_COLUMNS, lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    return path, len(rows)
