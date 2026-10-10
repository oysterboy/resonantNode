"""24 h unattended soak on one Emitter -> Analyzer pair (NODE-013, step 3a).

Every --interval-min minutes, on a fixed grid from the start time, one block
on the Analyzer:
  T3:  EMIT MODE REMOTE ; SEQ DIAG off ; SEQ start profile=TonalPulseScalar tries=10 ...
  OBS: EMIT MODE REMOTE ; SEQ DIAG off ; SEQ OBS start tries=5 period=2000 window=1800 ...
EMIT MODE REMOTE goes before every run: the Emitter boots in AUTO (ANA-005).
Each run is its own log and session.json run entry (T3_bNNN_HHMM,
OBS_bNNN_HHMM), so index.csv and seqcmp.py --table keep working; each block
appends one row to soak.csv. A serial error, timeout or board reset goes
into the row's error column; the port is reopened with backoff and the
next block starts on schedule. Ctrl+C ends cleanly.

  python tools/bench/soak.py --port COM3 --distance-cm 70 \
    --orientation "speaker faces mic, same height" --note "E3 COM4 -> E2 COM3, 0.3 FS"
  python tools/bench/soak.py --dry-run --distance-cm 70
  python tools/bench/soak.py --selfcheck bench/sessions/<s>/T3_scalar.<fw>.log [OBS.log]

The Analyzer is reset once at the start (BUILD banner -> firmware tag);
--no-reset skips that and takes the firmware from the session's last run.
"""
import argparse
import csv
import datetime
import os
import re
import statistics
import sys
import time

import serial

sys.path.insert(0, os.path.dirname(__file__))
import benchlib  # noqa: E402

T3_CMDS = ["EMIT MODE REMOTE", "SEQ DIAG off",
           "SEQ start profile=TonalPulseScalar tries=10 mode=detail when=all verbose=1"]
OBS_CMDS = ["EMIT MODE REMOTE", "SEQ DIAG off",
            "SEQ OBS start tries=5 period=2000 window=1800 profile=TonalPulseScalar "
            "mode=detail when=all verbose=1"]
CSV_COLUMNS = ("block", "start", "firmware", "t3_completed", "t3_expected", "t3_rejected",
               "t3_miss", "t3_late", "t3_early", "t3_detector_accepted", "t3_avg_strength",
               "t3_median_strength", "t3_avg_dt_ms", "dropped_dma_buffers", "obs_completed",
               "obs_detector_accepted", "error", "t3_log", "obs_log")
DROPPED = re.compile(r"dropped_dma_buffers=(\d+)")
BACKOFF_S = (2, 4, 8, 15, 30, 60)

ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
ap.add_argument("--port", help="Analyzer serial port, e.g. COM3")
ap.add_argument("--baud", type=int, default=115200)
ap.add_argument("--session", help="default bench/sessions/<YYYY-MM-DD>-soak24h-<distance>cm")
ap.add_argument("--purpose", default="NODE-013 24 h soak (step 3a, issue #27)")
ap.add_argument("--distance-cm", type=float, default=70.0)
ap.add_argument("--orientation", help="e.g. 'speaker faces mic, same height'")
ap.add_argument("--emitter-fw", help="emitter firmware git hash, if known")
ap.add_argument("--note", action="append", default=[], help="free-form setup note (repeatable)")
ap.add_argument("--hours", type=float, default=24.0)
ap.add_argument("--interval-min", type=float, default=15.0)
ap.add_argument("--timeout", type=int, default=120,
                help="per-run timeout floor in seconds; raised to fit tries x period (ANA-006)")
ap.add_argument("--no-reset", action="store_true",
                help="skip the one reset at the start (firmware read from the session's last run)")
ap.add_argument("--dry-run", action="store_true", help="print schedule and commands, open nothing")
ap.add_argument("--selfcheck", nargs="+", metavar="LOG",
                help="offline: parse a T3 log (and optional OBS log) into a soak.csv row and print it")
args = ap.parse_args()


def run_metrics(path):
    """Summary, source_strength values of trials that carry one, last dropped_dma_buffers."""
    parsed = benchlib.parse_run_log(path)
    strengths = [float(t["source_strength"]) for t in parsed["trials"]
                 if float(t.get("source_strength", "0") or 0) > 0]
    with open(path, encoding="utf-8", errors="replace") as f:
        dropped = [int(v) for v in DROPPED.findall(f.read())]
    # dropped_dma_buffers is cumulative since the run's "AUDIO stats reset" (SEQ start),
    # so the last value is the run's total.
    return benchlib.summarize_run(path), strengths, (dropped[-1] if dropped else None)


def block_row(t3_path, obs_path):
    row = {}
    dropped = []
    if t3_path and os.path.isfile(t3_path):
        s, strengths, d = run_metrics(t3_path)
        for col, key in (("t3_completed", "completed"), ("t3_expected", "expected_trials"),
                         ("t3_rejected", "rejected_trials"), ("t3_miss", "miss_trials"),
                         ("t3_late", "late_trials"), ("t3_early", "early_trials"),
                         ("t3_detector_accepted", "detector_accepted_trials"),
                         ("t3_avg_strength", "avg_strength"), ("t3_avg_dt_ms", "avg_dt_ms")):
            row[col] = s.get(key, "")
        row["t3_median_strength"] = f"{statistics.median(strengths):.1f}" if strengths else ""
        row["t3_log"] = os.path.basename(t3_path)
        dropped.append(d)
    if obs_path and os.path.isfile(obs_path):
        s, _, d = run_metrics(obs_path)
        row["obs_completed"] = s.get("completed", "")
        row["obs_detector_accepted"] = s.get("detector_accepted_trials", "")
        row["obs_log"] = os.path.basename(obs_path)
        dropped.append(d)
    if any(d is not None for d in dropped):
        row["dropped_dma_buffers"] = str(sum(d for d in dropped if d is not None))
    return row


def find_banner(text):
    """Firmware from a BUILD banner anywhere in text (boot junk may precede it on the line)."""
    i = text.find("BUILD role=")
    return benchlib.parse_banner(text[i:]) if i >= 0 else None


if args.selfcheck:
    row = block_row(args.selfcheck[0], args.selfcheck[1] if len(args.selfcheck) > 1 else None)
    with open(args.selfcheck[0], encoding="utf-8", errors="replace") as f:
        row["firmware"] = (benchlib.parse_banner(f.read()) or {}).get("git", "")
    w =csv.DictWriter(sys.stdout, fieldnames=CSV_COLUMNS, lineterminator="\n")
    w.writeheader()
    w.writerow(row)
    sys.exit(0)

dist = f"{args.distance_cm:g}"
session_dir = os.path.abspath(args.session) if args.session else os.path.join(
    benchlib.BENCH_ROOT, "sessions", f"{datetime.date.today().isoformat()}-soak24h-{dist}cm")
csv_path = os.path.join(session_dir, "soak.csv")
interval_s = args.interval_min * 60
n_blocks = int(round(args.hours * 60 / args.interval_min))
t3_timeout = benchlib.run_timeout(T3_CMDS[-1], args.timeout)
obs_timeout = benchlib.run_timeout(OBS_CMDS[-1], args.timeout)

# Continue the block numbering when restarted into the same session.
first_block = 1
if os.path.isfile(csv_path):
    with open(csv_path, encoding="utf-8") as f:
        done = [int(r["block"]) for r in csv.DictReader(f) if r.get("block", "").isdigit()]
    first_block = max(done, default=0) + 1

if args.dry_run:
    t_start = datetime.datetime.now()
    print(f"session:  {session_dir}")
    print(f"soak.csv: {csv_path} (first block {first_block})")
    print(f"port:     {args.port or '(none given)'} @ {args.baud}; "
          f"{'no reset' if args.no_reset else 'one reset + emitter warmup at start'}")
    print(f"schedule: {n_blocks} blocks every {args.interval_min:g} min over {args.hours:g} h, "
          f"grid from start")
    print(f"per block: T3 timeout {t3_timeout}s, OBS timeout {obs_timeout}s "
          f"(worst case {t3_timeout + obs_timeout + 10}s of {interval_s:g}s)")
    for name, cmds in (("T3", T3_CMDS), ("OBS", OBS_CMDS)):
        for c in cmds:
            print(f"  {name:3} >> {c}")
    for k in range(n_blocks):
        if 3 <= k < n_blocks - 2:
            if k == 3:
                print("  ...")
            continue
        at = t_start + datetime.timedelta(seconds=k * interval_s)
        b = first_block + k
        print(f"  block {b:3}  {at.isoformat(timespec='seconds')}  "
              f"T3_b{b:03d}_{at:%H%M}  OBS_b{b:03d}_{at:%H%M}")
    sys.exit(0)

if not args.port:
    sys.exit("--port is required (or use --dry-run / --selfcheck)")

os.makedirs(session_dir, exist_ok=True)
if os.path.exists(os.path.join(session_dir, "session.json")):
    data = benchlib.load_session(session_dir)
else:
    data = {
        "purpose": args.purpose,
        "created": datetime.datetime.now().isoformat(timespec="seconds"),
        "setup": {
            "distance_cm": args.distance_cm,
            "orientation": args.orientation or "",
            "emitter_fw": args.emitter_fw or "",
            "notes": args.note,
        },
        "runs": [],
    }
    benchlib.save_session(session_dir, data)


def log(msg):
    line = f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] {msg}"
    print(line, flush=True)
    with open(os.path.join(session_dir, "console.log"), "a", encoding="utf-8") as f:
        f.write(line + "\n")


def open_port():
    p = serial.Serial()
    p.port, p.baudrate, p.timeout = args.port, args.baud, 0.2
    p.dtr = False
    p.rts = False
    p.open()
    return p


def close_port():
    global port
    try:
        if port is not None:
            port.close()
    except (serial.SerialException, OSError):
        pass
    port = None


def reopen(deadline):
    """Reopen with backoff until deadline; re-send EMIT MODE REMOTE. Returns an error or ''."""
    global port, firmware, need_warmup
    close_port()
    attempt = 0
    while True:
        try:
            port = open_port()
            text = benchlib.read_for(port, 2)
            fw = find_banner(text)
            if fw:
                log(f"boot banner after reopen: {fw}")
                firmware, need_warmup = fw, True
            port.write(b"EMIT MODE REMOTE\n")
            log("reopened; " + benchlib.read_for(port, 1).strip().replace("\n", " | "))
            return ""
        except (serial.SerialException, OSError) as e:
            close_port()
            wait = BACKOFF_S[min(attempt, len(BACKOFF_S) - 1)]
            attempt += 1
            if time.time() + wait >= deadline:
                return f"port unavailable ({e})"
            log(f"reopen failed ({e}); retry in {wait}s")
            time.sleep(wait)


def capture(run_name, cmds, timeout_s, block_start, paths, kind):
    """One SEQ run into its log (paths[kind]) and a session.json entry, also when cut short.

    Returns the error text ('' when SEQ_SUMMARY arrived and nothing else went wrong).
    """
    global firmware, need_warmup
    body, err = [], ""
    started = datetime.datetime.now().isoformat(timespec="seconds")
    fw = firmware
    t0 = time.time()
    try:
        benchlib.run_commands(port, cmds, timeout_s, body, abort_on="BUILD role=")
    except KeyboardInterrupt:
        err = "interrupted"
        raise
    except (serial.SerialException, OSError) as e:
        err = f"serial error ({e})"
    finally:
        text = "".join(body).replace("\r\n", "\n")
        reset_fw = find_banner(text)
        if reset_fw:
            firmware, need_warmup = reset_fw, True
            err = (err + "; " if err else "") + f"board reset (git={reset_fw['git']})"
        elif not err and "SEQ_SUMMARY" not in text:
            err = f"timeout: no SEQ_SUMMARY within {timeout_s}s"
        fname = f"{run_name}.{fw['git']}.log"
        path = os.path.join(session_dir, fname)
        header = [f"# run: {run_name}", f"# firmware: role={fw['role']} git={fw['git']} "
                  f"version={fw['version']} board={fw['board']}", f"# started: {started}",
                  f"# soak block start: {block_start}"]
        if err:
            header.append(f"# soak error: {err}")
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(header) + "\n" + text)
        paths[kind] = path
        data["runs"].append({"name": run_name, "file": fname, "firmware": fw, "commands": cmds,
                             "started": started, "duration_s": round(time.time() - t0),
                             "summary": benchlib.summarize_run(path)})
        benchlib.save_session(session_dir, data)
    return err


def append_row(row):
    new = not os.path.isfile(csv_path)
    with open(csv_path, "a", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=CSV_COLUMNS, lineterminator="\n")
        if new:
            w.writeheader()
        w.writerow(row)
        f.flush()




def run_block(b, slot, row, errors):
    """Run T3 then OBS; fill row and errors even when cut short (serial error, Ctrl+C)."""
    global need_warmup
    paths = {}
    hhmm = slot.strftime("%H%M")
    try:
        for kind, cmds, timeout_s in (("T3", T3_CMDS, t3_timeout), ("OBS", OBS_CMDS, obs_timeout)):
            if need_warmup:
                log("warmup: " + benchlib.warmup_emitter(port).strip().replace("\n", " | "))
                need_warmup = False
            name = f"{kind}_b{b:03d}_{hhmm}"
            log(f"start {name}")
            err = capture(name, cmds, timeout_s, slot.isoformat(timespec="seconds"), paths, kind)
            if err:
                errors.append(f"{kind}: {err}")
                log(f"ERROR {name}: {err}")
                if "serial error" in err:
                    close_port()
                    break
                if err.startswith("timeout"):
                    port.write(b"SEQ stop\n")
            benchlib.read_for(port, 2)
    except (serial.SerialException, OSError) as e:
        errors.append(f"serial error ({e})")
        close_port()
    except KeyboardInterrupt:
        errors.append("interrupted")
        raise
    finally:
        row.update(block_row(paths.get("T3"), paths.get("OBS")))


def wait_for_slot(slot_t, errors):
    """Idle until slot_t; keep the port drained and catch a reset or port loss while idle."""
    global firmware, need_warmup
    while time.time() < slot_t:
        left = max(0.0, slot_t - time.time())
        if port is None:
            time.sleep(min(1.0, left))
            continue
        try:
            fw = find_banner(benchlib.read_for(port, min(1.0, left)))
        except (serial.SerialException, OSError) as e:
            errors.append(f"serial error while idle ({e})")
            log(errors[-1])
            err = reopen(slot_t)
            if err:
                errors.append(err)
            continue
        if fw:
            firmware, need_warmup = fw, True
            errors.append(f"board reset while idle (git={fw['git']})")
            log(errors[-1])


port = None
need_warmup = False
firmware = None
data.setdefault("soak", []).append({
    "started": datetime.datetime.now().isoformat(timespec="seconds"),
    "port": args.port, "hours": args.hours, "interval_min": args.interval_min,
    "first_block": first_block, "blocks": n_blocks, "csv": "soak.csv",
    "t3_commands": T3_CMDS, "obs_commands": OBS_CMDS,
})
benchlib.save_session(session_dir, data)

try:
    port = open_port()
    if not args.no_reset:
        firmware = find_banner(benchlib.reset_board(port))
        log(f"boot banner: {firmware}")
        need_warmup = True
    else:
        firmware = data["runs"][-1]["firmware"] if data["runs"] else None
        log(f"no reset; firmware from session: {firmware}")
    if firmware is None:
        sys.exit("no BUILD banner seen; cannot tag runs with firmware (reset the board or check the port)")

    t_start = time.time()
    for k in range(n_blocks):
        b = first_block + k
        slot_t = t_start + k * interval_s  # fixed grid: a late block never shifts the next one
        slot = datetime.datetime.fromtimestamp(slot_t)
        row, errors = {"block": b, "start": slot.isoformat(timespec="seconds")}, []
        if time.time() > slot_t + interval_s:
            row.update(firmware=firmware["git"], error="skipped: previous block overran this slot")
            append_row(row)
            log(f"block {b} skipped")
            continue
        try:
            wait_for_slot(slot_t, errors)
            row["start"] = datetime.datetime.now().isoformat(timespec="seconds")
            if port is None:
                err = reopen(slot_t + interval_s - 60)
                if err:
                    errors.append(err)
            if port is not None:
                log(f"block {b}/{first_block + n_blocks - 1}")
                run_block(b, slot, row, errors)
        finally:
            row["firmware"] = firmware["git"]
            row["error"] = "; ".join(errors)
            append_row(row)
            log(f"block {b}: T3 expected {row.get('t3_expected', '-')}/{row.get('t3_completed', '-')} "
                f"median {row.get('t3_median_strength', '-')} drops {row.get('dropped_dma_buffers', '-')} "
                f"OBS det {row.get('obs_detector_accepted', '-')}"
                + (f" ERROR {row['error']}" if row["error"] else ""))
        if port is None and k + 1 < n_blocks:
            err = reopen(t_start + (k + 1) * interval_s)
            if err:
                log(err)
except KeyboardInterrupt:
    log("interrupted (Ctrl+C); stopping")
    try:
        if port is not None:
            port.write(b"SEQ stop\n")
    except (serial.SerialException, OSError):
        pass
finally:
    close_port()
    path, n = benchlib.rebuild_index()
    log(f"soak ended; session updated, index rebuilt ({n} rows)")
