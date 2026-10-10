"""Shared parsing for bench session logs, plus the serial exchange used by
seqrun.py and soak.py (stdlib only; the serial helpers take an open pyserial port)."""
import csv
import json
import os
import re
import time

KV = re.compile(r"(\S+?)=(\S*)")
# Board boot banner, or the "# firmware:" header seqrun/import write into run logs.
# board= was added 2026-10-09 (D-AMP); every earlier firmware was piezo-only.
BANNER = re.compile(r"^(?:BUILD|# firmware:) role=(\S+) git=(\S+) .*version=(\S+)(?: board=(\S+))?")
LINE_TYPES = ("SEQ_TRIAL", "SEQ_SOURCE_CORE", "SEQ_SOURCE_SPEC", "SEQ_SOURCE",
              "SEQ_INSPECT", "SEQ_EXPLAIN", "SEQ_DETAIL", "SEQ_SUMMARY", "SEQ REPORT")

# SEQ_SUMMARY fields copied into summaries and the index, in this order.
SUMMARY_FIELDS = ("trials", "completed", "expected_trials", "early_trials", "late_trials",
                  "miss_trials", "duplicate_trials", "unexpected_trials", "rejected_trials",
                  "buffer_overrun_trials", "detector_accepted_trials", "detector_reject_trials",
                  "pattern_valid_trials", "pattern_rejected_trials", "avg_dt_ms",
                  "avg_strength", "avg_conf")

INDEX_COLUMNS = ("session", "run", "date", "firmware", "role", "profile", "detector",
                 "distance_cm", "purpose", "complete") + SUMMARY_FIELDS

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
            return {"role": m.group(1), "git": m.group(2), "version": m.group(3),
                    "board": m.group(4) or "piezo"}
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
    if not s and parsed["trials"]:
        # Cut before SEQ_SUMMARY (ANA-006): keep what the trial lines say.
        out["trials"] = str(len(parsed["trials"]))
        for r, n in results.items():
            out[f"{r}_trials"] = str(n)
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
            # Re-read the log so a summarize_run change reaches old runs too.
            log_path = os.path.join(sdir, run["file"])
            summ = summarize_run(log_path) if os.path.isfile(log_path) else run.get("summary", {})
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
                "complete": "1" if summ.get("complete") else "0",
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


def read_for(ser, seconds):
    end = time.time() + seconds
    buf = b""
    while time.time() < end:
        buf += ser.read(65536)
    return buf.decode(errors="replace")


def run_timeout(command, floor):
    """Seconds a SEQ run may take: tries x (period + per-trial overhead) + slack.

    A fixed timeout cut 150- and 400-window OBS runs before SEQ_SUMMARY
    (ANA-006). Per-trial overhead measured 2026-10-09: ~0.9 s over period.
    """
    tries = re.search(r"(?:tries=|start )(\d+)", command)
    period = re.search(r"period=(\d+)", command)
    n = int(tries.group(1)) if tries else 50
    period_s = int(period.group(1)) / 1000.0 if period else 2.4
    return max(floor, int(n * (period_s + 1.5)) + 60)


def reset_board(ser, seconds=4):
    """Pulse RTS (EN) and return what the board prints while booting."""
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False
    return read_for(ser, seconds)


def warmup_emitter(ser):
    """The first emitter claim after boot tends to time out; spend it here."""
    ser.write(b"EMIT CHIRP freq=3200 dur=100\n")
    return read_for(ser, 2)


def run_commands(ser, cmds, timeout_s, body=None, abort_on=None):
    """Send cmds[:-1], then the SEQ start cmds[-1]; read until SEQ_SUMMARY + 3 s or timeout.

    The transcript goes into body (a list, returned), so a caller that passes
    its own list keeps what was read when the port fails mid-run. abort_on
    (e.g. "BUILD role=", a reboot) ends the wait early.
    """
    body = [] if body is None else body
    for c in cmds[:-1]:
        ser.write((c + "\n").encode())
        body.append(f">> {c}\n" + read_for(ser, 0.8))
    ser.write((cmds[-1] + "\n").encode())
    body.append(f">> {cmds[-1]}\n")
    t0 = time.time()
    done_at = None
    while time.time() - t0 < timeout_s:
        body.append(read_for(ser, 1))
        if abort_on and abort_on in body[-2] + body[-1]:
            body.append(read_for(ser, 3))  # rest of the boot output
            break
        if done_at is None and "SEQ_SUMMARY" in body[-2] + body[-1]:
            done_at = time.time()
        if done_at is not None and time.time() - done_at > 3:
            break
    return body
