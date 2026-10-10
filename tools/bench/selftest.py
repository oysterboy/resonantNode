"""Run the firmware SELFTEST on every attached board and log it to a bench session (NODE-015, step 3b).

Per port: open without resetting (DTR/RTS held low; --reset pulses EN and
reads the BUILD banner), send SELFTEST, parse the one-line-per-check output
(format: docs/refactors/selftest.md), map the MAC to the board label from
the table in bench/README.md, and print one row per board. With --external
each Node then gets `SELFTEST external wait_ms=N` while the next board in
the list is told `SELFTEST chirp n=K` (rotating), so every Node proves it
hears someone else's chirp.

  python tools/bench/selftest.py                      # every CP210x port
  python tools/bench/selftest.py --ports COM3 COM4 --external --slug pair
  python tools/bench/selftest.py --dry-run --ports COM3 COM4 --external
  python tools/bench/selftest.py --selfcheck tools/bench/samples/selftest_node.sample.log

Logs: bench/sessions/<YYYY-MM-DD>-selftest[-slug]/selftest_<label>_<HHMMSS>.<git>.log,
one per board, plus session.json. SELFTEST entries live under the session's
"selftest" key, not "runs": index.csv is one row per SEQ run with
SEQ_SUMMARY fields, which a self-test does not have, so rebuild_index()
skips them and keeps working unchanged.

The amp tone level is compared with the newest earlier SELFTEST of the same
MAC in bench/sessions (damp-bench-check.md AC rule: tone not more than 10 dB
below the reference); "tone_vs_prev" in the table, DEGRADED when below.
"""
import argparse
import datetime
import glob
import json
import math
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
import benchlib  # noqa: E402

CP210X_VID_PID = (0x10C4, 0xEA60)
CHECKS = ("board", "mic", "amp", "quiet_boot", "own_chirp_suppressed", "external_chirp")
SHORT = {"board": "board", "mic": "mic", "amp": "amp", "quiet_boot": "qboot",
         "own_chirp_suppressed": "ownchirp", "external_chirp": "extern"}
LINE = re.compile(r"(SELFTEST(?:_[A-Z]+)?) ?(.*)$")
BOARD_ROW = re.compile(r"^\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|\s*([0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5})\s*\|")
TONE_DROP_LIMIT_DB = -10.0  # AC rule: chirp level not more than 10 dB below the reference
FULL_TIMEOUT_S = 120
CHIRP_N = 5
CHIRP_GAP_MS = 1500
QUIET_EMITTERS = "EMIT MODE REMOTE"


def parse_args(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--ports", nargs="+", help="serial ports, e.g. COM3 COM4 (default: every CP210x port)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--reset", action="store_true",
                    help="pulse EN on each board first and read its BUILD banner (default: no reset)")
    ap.add_argument("--external", action="store_true",
                    help="after the full pass, run external_chirp on every Node with the next board chirping")
    ap.add_argument("--external-wait-ms", type=int, default=10000)
    ap.add_argument("--chirps", type=int, default=CHIRP_N, help="chirps the helper board emits per external test")
    ap.add_argument("--session", help="session folder (default bench/sessions/<date>-selftest[-slug])")
    ap.add_argument("--slug", help="session folder suffix, e.g. 'pair' -> <date>-selftest-pair")
    ap.add_argument("--purpose", default="NODE-015 SELFTEST (step 3b, issue #28)")
    ap.add_argument("--distance-cm", type=float, help="board spacing, if it matters for the session")
    ap.add_argument("--note", action="append", default=[], help="free-form setup note (repeatable)")
    ap.add_argument("--timeout", type=int, default=FULL_TIMEOUT_S, help="seconds per full SELFTEST")
    ap.add_argument("--no-compare", action="store_true", help="skip the tone comparison with earlier sessions")
    ap.add_argument("--keep-emitters", action="store_true",
                    help=f"do not send '{QUIET_EMITTERS}' to every port first (Emitters stay in AUTO)")
    ap.add_argument("--dry-run", action="store_true", help="print the plan, open nothing")
    ap.add_argument("--selfcheck", nargs="+", metavar="LOG",
                    help="offline: parse SELFTEST log(s) and print the table (no ports, no session)")
    return ap.parse_args(argv)


# --- parsing -----------------------------------------------------------------

def parse_selftest(text):
    """Checks, summary, amp trials and markers from a transcript (later lines win)."""
    out = {"checks": {}, "summary": {}, "summaries": [], "amp_trials": [], "begin": [], "markers": []}
    for raw in text.splitlines():
        stripped = raw.strip()
        if stripped.startswith("#") or stripped.startswith(">>"):
            continue  # log header, runner notes, echoed commands
        i = stripped.find("SELFTEST")  # boot junk may precede it on the line
        if i < 0:
            continue
        m = LINE.match(stripped[i:])
        if not m:
            continue
        kind, rest = m.group(1), m.group(2)
        kv = dict(benchlib.KV.findall(rest))
        if kind == "SELFTEST" and "check" in kv:
            out["checks"][kv["check"]] = kv
        elif kind == "SELFTEST_SUMMARY":
            out["summaries"].append(kv)
        elif kind == "SELFTEST_AMP":
            out["amp_trials"].append(kv)
        elif kind == "SELFTEST_BEGIN":
            out["begin"].append(kv)
        elif kind != "SELFTEST":
            out["markers"].append((kind, rest))
    if out["summaries"]:
        # One SELFTEST (full) plus maybe one SELFTEST external: FAIL if either failed.
        total = {k: sum(int(s.get(k, 0) or 0) for s in out["summaries"]) for k in ("pass", "fail", "skip")}
        out["summary"] = {"result": "FAIL" if any(s.get("result") != "PASS" for s in out["summaries"])
                          else "PASS", **{k: str(v) for k, v in total.items()},
                          "runs": str(len(out["summaries"])), "role": out["summaries"][-1].get("role", "?")}
    return out


def firmware_from(parsed, text=""):
    b = parsed["checks"].get("board")
    if b:
        return {"role": b.get("role", "?"), "git": b.get("git", "unknown"),
                "version": b.get("version", "?"), "board": b.get("board", "?")}
    i = text.find("BUILD role=")
    return benchlib.parse_banner(text[i:]) if i >= 0 else None


def load_board_table(readme=os.path.join(benchlib.BENCH_ROOT, "README.md")):
    """MAC (lower case) -> label, from the Boards table in bench/README.md."""
    table = {}
    try:
        with open(readme, encoding="utf-8") as f:
            for line in f:
                m = BOARD_ROW.match(line.strip())
                if m:
                    table[m.group(3).lower()] = m.group(1)
    except OSError:
        pass
    return table


def label_for(mac, table):
    return table.get((mac or "").lower(), "?")


def prev_tone(mac, before_iso, exclude_dir=None):
    """Newest amp tone of an earlier SELFTEST of this MAC: (tone, session, started) or None."""
    best = None
    for sj in glob.glob(os.path.join(benchlib.BENCH_ROOT, "sessions", "*", "session.json")):
        sdir = os.path.dirname(sj)
        if exclude_dir and os.path.abspath(sdir) == os.path.abspath(exclude_dir):
            continue
        try:
            data = benchlib.load_session(sdir)
        except (OSError, ValueError):
            continue
        for e in data.get("selftest", []):
            if (e.get("mac") or "").lower() != (mac or "").lower():
                continue
            amp = e.get("checks", {}).get("amp", {})
            if amp.get("result") != "PASS" or not amp.get("tone"):
                continue
            started = e.get("started", "")
            if before_iso and started >= before_iso:
                continue
            if best is None or started > best[2]:
                best = (float(amp["tone"]), os.path.basename(sdir), started)
    return best


def row_for(label, port, parsed, firmware, compare=None):
    c = parsed["checks"]
    b = c.get("board", {})
    amp = c.get("amp", {})
    row = {"label": label, "port": port or "-", "mac": b.get("mac", "?"),
           "role": (firmware or {}).get("role", "?"), "hw": (firmware or {}).get("board", "?"),
           "git": (firmware or {}).get("git", "?"),
           "result": parsed["summary"].get("result", "INCOMPLETE")}
    for name in CHECKS:
        row[SHORT[name]] = c.get(name, {}).get("result", "-")
    row["snr_db"] = amp.get("snr_db", "-")
    row["tone"] = amp.get("tone", "-")
    row["floor"] = amp.get("floor", "-")
    row["tone_vs_prev"] = "-"
    if compare and amp.get("tone"):
        delta = 20.0 * math.log10(max(float(amp["tone"]), 1.0) / max(compare[0], 1.0))
        row["tone_vs_prev"] = f"{delta:+.1f}dB" + (" DEGRADED" if delta < TONE_DROP_LIMIT_DB else "")
    return row


def print_table(rows):
    cols = ["label", "port", "mac", "role", "hw", "git", "result"] + [SHORT[n] for n in CHECKS] + \
           ["snr_db", "tone", "floor", "tone_vs_prev"]
    widths = {k: max(len(k), *(len(str(r.get(k, ""))) for r in rows)) for k in cols}
    print("  ".join(k.ljust(widths[k]) for k in cols))
    for r in rows:
        print("  ".join(str(r.get(k, "")).ljust(widths[k]) for k in cols))


# --- offline modes ----------------------------------------------------------

def selfcheck(paths):
    table = load_board_table()
    rows = []
    for p in paths:
        with open(p, encoding="utf-8", errors="replace") as f:
            text = f.read()
        parsed = parse_selftest(text)
        fw = firmware_from(parsed, text)
        mac = parsed["checks"].get("board", {}).get("mac")
        rows.append(row_for(label_for(mac, table), None, parsed, fw))
        print(f"== {p}")
        print(f"firmware: {fw}")
        for name, kv in parsed["checks"].items():
            print(f"  {name:22} {kv.get('result', '?'):5} "
                  + " ".join(f"{k}={v}" for k, v in kv.items() if k not in ("check", "result")))
        print(f"  amp trials: {len(parsed['amp_trials'])}  markers: {[m[0] for m in parsed['markers']]}")
        print(f"  summary: {parsed['summary']}")
        problems = [n for n in ("board", "mic", "amp") if n not in parsed["checks"]]
        if problems:
            print(f"  WARNING: missing checks {problems}")
        if not parsed["summary"]:
            print("  WARNING: no SELFTEST_SUMMARY (cut short?)")
    print()
    print_table(rows)
    return 0


def detect_ports():
    from serial.tools import list_ports  # enumerates only, opens nothing
    return [p.device for p in list_ports.comports() if (p.vid, p.pid) == CP210X_VID_PID]


def session_path(args):
    if args.session:
        return os.path.abspath(args.session)
    name = f"{datetime.date.today().isoformat()}-selftest" + (f"-{args.slug}" if args.slug else "")
    return os.path.join(benchlib.BENCH_ROOT, "sessions", name)


def dry_run(args):
    ports = args.ports
    if not ports:
        try:
            ports = detect_ports()
            src = "detected CP210x"
        except ImportError:
            ports, src = [], "pyserial missing"
    else:
        src = "--ports"
    print(f"session:  {session_path(args)}")
    print(f"ports:    {ports or '(none)'} ({src}) @ {args.baud}")
    print(f"open:     DTR/RTS held low (no auto-reset); "
          f"{'EN pulse + BUILD banner per board' if args.reset else 'no reset'}")
    print(f"boards:   MAC -> label from bench/README.md: {load_board_table() or '(table empty)'}")
    if not args.keep_emitters:
        print(f"first:    >> {QUIET_EMITTERS}   on every port (AUTO Emitters stop chirping)")
    print("pass 1, one board at a time:")
    for p in ports:
        print(f"  {p}: >> SELFTEST   (until SELFTEST_SUMMARY, timeout {args.timeout}s)")
    if args.external:
        print("pass 2, external_chirp rotation (only boards whose SELFTEST reported role=node):")
        if len(ports) < 2:
            print("  needs at least two boards; skipped")
        for i, p in enumerate(ports if len(ports) >= 2 else []):
            helper = ports[(i + 1) % len(ports)]
            print(f"  {p}: >> SELFTEST external wait_ms={args.external_wait_ms}"
                  f"   then on 'SELFTEST_EXTERNAL waiting': {helper}: >> SELFTEST chirp n={args.chirps}"
                  f" gap_ms={CHIRP_GAP_MS}")
    print("then: one log per board, session.json 'selftest' entries, index rebuilt (SEQ runs only)")
    if not args.no_compare:
        print(f"amp tone compared with the newest earlier SELFTEST per MAC (flag below {TONE_DROP_LIMIT_DB:g} dB)")
    return 0


# --- live run -----------------------------------------------------------------

class Board:
    def __init__(self, port_name, ser):
        self.port_name = port_name
        self.ser = ser
        self.chunks = []
        self.firmware = None
        self.parsed = None
        self.started = datetime.datetime.now().isoformat(timespec="seconds")
        self.t0 = time.time()

    def text(self):
        return "".join(self.chunks).replace("\r\n", "\n")

    def send(self, cmd):
        self.chunks.append(f"\n>> {cmd}\n")
        self.ser.write((cmd + "\n").encode())


def pump(boards, seconds):
    """Read every port for `seconds`, appending to each board's transcript."""
    end = time.time() + seconds
    while True:
        for b in boards:
            data = b.ser.read(b.ser.in_waiting or 1)
            if data:
                b.chunks.append(data.decode(errors="replace"))
        if time.time() >= end:
            return


def wait_for(boards, board, marker, timeout_s, since):
    """Pump until `marker` appears in board's transcript after offset `since`."""
    end = time.time() + timeout_s
    while time.time() < end:
        pump(boards, 0.3)
        if marker in "".join(board.chunks)[since:]:
            return True
    return False


def live(args):
    import serial  # pyserial, only needed here

    ports = args.ports or detect_ports()
    if not ports:
        sys.exit("no ports given and no CP210x port found")
    session_dir = session_path(args)
    table = load_board_table()

    boards = []
    try:
        for p in ports:
            s = serial.Serial()
            s.port, s.baudrate, s.timeout = p, args.baud, 0.05
            s.dtr = False  # set before open, or the CP210x auto-reset circuit resets the board
            s.rts = False
            s.open()
            boards.append(Board(p, s))
        if args.reset:
            for b in boards:
                b.chunks.append(benchlib.reset_board(b.ser))
        pump(boards, 0.5)
        if not args.keep_emitters:
            # An Emitter boots in AUTO (a chirp every 2 s, ANA-005), which would land in
            # every other board's mic and amp windows. The Analyzer forwards EMIT to its
            # linked Emitter; an Emitter takes EMIT on its own USB console; a Node ignores it.
            for b in boards:
                b.send(QUIET_EMITTERS)
            pump(boards, 3.0)

        # Pass 1: full SELFTEST, one board at a time so test chirps do not overlap.
        for b in boards:
            since = len("".join(b.chunks))
            b.send("SELFTEST")
            print(f"[{time.strftime('%H:%M:%S')}] {b.port_name}: SELFTEST ...", flush=True)
            ok = wait_for(boards, b, "SELFTEST_SUMMARY", args.timeout, since)
            pump(boards, 0.5)
            b.parsed = parse_selftest(b.text())
            b.firmware = firmware_from(b.parsed, b.text())
            if not ok:
                b.chunks.append(f"\n# selftest runner: no SELFTEST_SUMMARY within {args.timeout}s\n")
            print(f"[{time.strftime('%H:%M:%S')}] {b.port_name}: "
                  f"{b.parsed['summary'].get('result', 'INCOMPLETE')}", flush=True)

        # Pass 2: external_chirp on each Node, the next board in the list chirping.
        if args.external and len(boards) >= 2:
            for i, a in enumerate(boards):
                if (a.firmware or {}).get("role") != "node":
                    continue
                helper = boards[(i + 1) % len(boards)]
                since = len("".join(a.chunks))
                a.send(f"SELFTEST external wait_ms={args.external_wait_ms}")
                if wait_for(boards, a, "SELFTEST_EXTERNAL waiting", 30, since):
                    helper.send(f"SELFTEST chirp n={args.chirps} gap_ms={CHIRP_GAP_MS}")
                else:
                    a.chunks.append("\n# selftest runner: Node never reported SELFTEST_EXTERNAL waiting\n")
                wait_for(boards, a, "SELFTEST_SUMMARY", args.external_wait_ms / 1000.0 + 30,
                         len("".join(a.chunks)))
                pump(boards, 1.0)
                a.parsed = parse_selftest(a.text())
                ext = a.parsed["checks"].get("external_chirp", {})
                print(f"[{time.strftime('%H:%M:%S')}] {a.port_name}: external_chirp "
                      f"{ext.get('result', '?')} (helper {helper.port_name})", flush=True)
    finally:
        for b in boards:
            try:
                b.ser.close()
            except Exception:  # noqa: BLE001 - closing best effort
                pass

    # Logs and session.json.
    os.makedirs(session_dir, exist_ok=True)
    if os.path.exists(os.path.join(session_dir, "session.json")):
        data = benchlib.load_session(session_dir)
    else:
        data = {"purpose": args.purpose,
                "created": datetime.datetime.now().isoformat(timespec="seconds"),
                "setup": {"distance_cm": args.distance_cm if args.distance_cm is not None else "",
                          "orientation": "", "emitter_fw": "", "notes": args.note},
                "runs": []}
    data.setdefault("selftest", [])
    rows = []
    for b in boards:
        text = b.text()
        parsed = b.parsed or parse_selftest(text)
        fw = b.firmware or firmware_from(parsed, text) or {"role": "?", "git": "unknown",
                                                           "version": "?", "board": "?"}
        mac = parsed["checks"].get("board", {}).get("mac", "")
        label = label_for(mac, table)
        compare = None if args.no_compare else prev_tone(mac, b.started, exclude_dir=None)
        rows.append(row_for(label, b.port_name, parsed, fw, compare))
        fname = f"selftest_{label if label != '?' else b.port_name}_{b.started[11:19].replace(':', '')}.{fw['git']}.log"
        header = [f"# run: selftest {label} ({b.port_name})",
                  f"# firmware: role={fw['role']} git={fw['git']} version={fw['version']} board={fw['board']}",
                  f"# started: {b.started}", f"# mac: {mac}", f"# port: {b.port_name}"]
        with open(os.path.join(session_dir, fname), "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(header) + "\n" + text)
        data["selftest"].append({
            "label": label, "mac": mac, "port": b.port_name, "file": fname, "firmware": fw,
            "started": b.started, "duration_s": round(time.time() - b.t0),
            "result": parsed["summary"].get("result", "INCOMPLETE"),
            "summary": parsed["summary"], "checks": parsed["checks"],
            "amp_trials": parsed["amp_trials"],
            "tone_reference": ({"tone": compare[0], "session": compare[1], "started": compare[2]}
                               if compare else None),
        })
    benchlib.save_session(session_dir, data)
    benchlib.rebuild_index()
    print()
    print_table(rows)
    print(f"\nsession: {session_dir}")
    return 0 if all(r["result"] == "PASS" for r in rows) else 1


def main(argv=None):
    args = parse_args(argv)
    if args.selfcheck:
        return selfcheck(args.selfcheck)
    if args.dry_run:
        return dry_run(args)
    return live(args)


if __name__ == "__main__":
    sys.exit(main())
