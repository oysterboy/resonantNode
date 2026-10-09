"""Run analyzer SEQ runs over serial into a bench session folder.

Each --run takes a name and one or more commands separated by ';'. The last
command is the SEQ start; earlier ones (e.g. 'SEQ DIAG off') are sent first.
The board is reset once at the start so the BUILD banner identifies the
firmware; every run is tagged with it.

Example:
  python tools/bench/seqrun.py --port COM6 \
    --session bench/sessions/2026-10-08-issue7-40cm \
    --purpose "issue #7 T2/T3 baseline" --distance-cm 40 \
    --run T2_freq "SEQ start profile=TonalPulseFreq tries=50 mode=detail when=all verbose=1"

Setup flags are only needed the first time a session is created; later
invocations into the same folder reuse them. Change the physical setup ->
start a new session folder.
"""
import argparse
import datetime
import os
import sys
import time

import serial

sys.path.insert(0, os.path.dirname(__file__))
import benchlib  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("--port", required=True)
ap.add_argument("--baud", type=int, default=115200)
ap.add_argument("--session", required=True, help="bench/sessions/<YYYY-MM-DD-slug>")
ap.add_argument("--purpose", help="why this session exists, e.g. 'issue #7 T2/T3'")
ap.add_argument("--distance-cm", type=float)
ap.add_argument("--orientation", help="e.g. 'vertical emit, vertical mic'")
ap.add_argument("--emitter-fw", help="emitter firmware git hash, if known")
ap.add_argument("--note", action="append", default=[], help="free-form setup note (repeatable)")
ap.add_argument("--run", nargs=2, action="append", metavar=("NAME", "COMMANDS"), required=True)
ap.add_argument("--no-reset", action="store_true", help="skip reset (firmware read from session)")
ap.add_argument("--timeout", type=int, default=600, help="per-run timeout seconds")
args = ap.parse_args()

session_dir = os.path.abspath(args.session)
os.makedirs(session_dir, exist_ok=True)
session_file = os.path.join(session_dir, "session.json")
if os.path.exists(session_file):
    data = benchlib.load_session(session_dir)
else:
    if args.distance_cm is None or not args.purpose:
        sys.exit("new session: --purpose and --distance-cm are required")
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

s = serial.Serial()
s.port, s.baudrate, s.timeout = args.port, args.baud, 0.2
s.dtr = False
s.rts = False
s.open()


def read_for(seconds):
    end = time.time() + seconds
    buf = b""
    while time.time() < end:
        buf += s.read(65536)
    return buf.decode(errors="replace")


def log(msg):
    line = f"[{time.strftime('%H:%M:%S')}] {msg}"
    print(line, flush=True)
    with open(os.path.join(session_dir, "console.log"), "a", encoding="utf-8") as f:
        f.write(line + "\n")


firmware = None
if not args.no_reset:
    s.rts = True
    time.sleep(0.1)
    s.rts = False
    boot = read_for(4)
    firmware = benchlib.parse_banner(boot)
    log(f"boot banner: {firmware}")
    # The first emitter claim after boot tends to time out; spend it here.
    s.write(b"EMIT CHIRP freq=3200 dur=100\n")
    log("warmup: " + read_for(2).strip().replace("\n", " | "))
if firmware is None:
    sys.exit("no BUILD banner seen; cannot tag runs with firmware (reset the board or check the port)")

for name, commands in args.run:
    cmds = [c.strip() for c in commands.split(";") if c.strip()]
    fname = f"{name}.{firmware['git']}.log"
    if any(r["file"] == fname for r in data["runs"]):
        sys.exit(f"run file {fname} already exists in this session; pick another name")
    started = datetime.datetime.now().isoformat(timespec="seconds")
    header = [f"# run: {name}", f"# firmware: role={firmware['role']} git={firmware['git']} "
              f"version={firmware['version']} board={firmware['board']}", f"# started: {started}"]
    body = []
    for c in cmds[:-1]:
        s.write((c + "\n").encode())
        body.append(f">> {c}\n" + read_for(0.8))
    s.write((cmds[-1] + "\n").encode())
    body.append(f">> {cmds[-1]}\n")
    log(f"start {name}: {' ; '.join(cmds)}")
    t0 = time.time()
    buf, done_at = "", None
    while time.time() - t0 < args.timeout:
        buf += read_for(1)
        if done_at is None and "SEQ_SUMMARY" in buf:
            done_at = time.time()
        if done_at is not None and time.time() - done_at > 3:
            break
    body.append(buf)
    path = os.path.join(session_dir, fname)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(header) + "\n" + "".join(body).replace("\r\n", "\n"))
    run = {"name": name, "file": fname, "firmware": firmware, "commands": cmds,
           "started": started, "duration_s": round(time.time() - t0),
           "summary": benchlib.summarize_run(path)}
    data["runs"].append(run)
    benchlib.save_session(session_dir, data)
    summ = run["summary"]
    log(f"done {name} ({run['duration_s']}s): complete={summ['complete']} "
        f"results={summ['trial_results']}")
    read_for(2)

s.close()
benchlib.rebuild_index()
log("session updated, index rebuilt")
