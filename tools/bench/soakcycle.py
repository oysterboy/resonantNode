"""Unattended long run with a room check every cycle (step 11c): Nodes run free, and every --cycle-min the run
silences them, lets the Analyzer measure the room with the Emitter, and lets them run again.

Network part: the --nodes boards run as an installation (output.silent set to 0, unsaved, so a reboot brings them
back silent), RB log full, RB STATUS every --status-every s. Their lines go to <name>.log as "<t> <NAME> <line>",
the format nodegroup.py writes, so nodegroup_interim.py summarises it (chains, noise starts, drops, resets,
drift per pair).

Room check, each cycle (owner 2026-10-26: "every hour, to catch room drift"): the Nodes are silenced (unsaved), and
after a settle the Analyzer runs each --step (a SEQ start line, 10 trials by default) and --raw-reps RAW hop
captures. Per cycle: cycle<NN>_<step>.log (the Analyzer transcript), cycle<NN>_hop5_r<k>.csv (RAW, readable by
tools/bench/hopbins.py), and one row in cycles.csv (local time, per step expected / rejected / miss, the reject
reasons, avg strength). Then the Nodes run again.

All ports are opened with DTR/RTS low (no reset) and held for the whole run.

  python tools/bench/soakcycle.py --nodes E1=COM8,E4=COM10,E5=COM11 --analyzer COM3 --hours 24 --cycle-min 60 \\
      --out bench/sessions/<folder> --name soak
"""
import argparse
import csv
import datetime
import os
import re
import time

import serial

HOP = "freq=3300 band=2950-3950 dur=25 pulses=5 gap=10 hop=2950,3200,3450,3700,3950"
DEFAULT_STEPS = [
    ("hop10", "SEQ start profile=TonalPulseScalar tries=10 %s mode=detail when=all verbose=1" % HOP),
    ("chirp3200_100", "SEQ start profile=TonalPulseScalar tries=10 dur=100 band=2950-3950 mode=detail when=all verbose=1"),
    ("chirp3450_150", "SEQ start profile=TonalPulseScalar tries=10 freq=3450 dur=150 band=2950-3950 mode=detail when=all verbose=1"),
]
RAW_HOP = ("RAW trigger f=3300 dur=25 pulses=5 gap=10 hop=2950,3200,3450,3700,3950 pre=50 post=250 mode=both "
           "band=2950-3950")

ap = argparse.ArgumentParser()
ap.add_argument("--nodes", required=True, help="NAME=PORT,...")
ap.add_argument("--analyzer", required=True, help="the Analyzer's port (the Emitter is on its link)")
ap.add_argument("--hours", type=float, default=24.0)
ap.add_argument("--cycle-min", type=float, default=60.0)
ap.add_argument("--first-check-min", type=float, default=2.0, help="the first room check, minutes after the start")
ap.add_argument("--status-every", type=float, default=120.0)
ap.add_argument("--settle-s", type=float, default=8.0, help="after silencing the Nodes, before the first step")
ap.add_argument("--raw-reps", type=int, default=2)
ap.add_argument("--out", required=True)
ap.add_argument("--name", default="soak")
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)


def open_port(p):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = p, 115200, 0
    s.dtr = False
    s.rts = False
    s.open()
    return s


nodes = [tuple(x.split("=")) for x in a.nodes.split(",") if x]
ports = {n: open_port(p) for n, p in nodes}
analyzer = open_port(a.analyzer)
partial = {n: b"" for n in ports}
partial["AN"] = b""
t0 = time.time()
log = open(os.path.join(a.out, a.name + ".log"), "w", encoding="utf-8")
log.write("# soakcycle nodes=%s analyzer=%s hours=%.2f cycle_min=%.1f started_unix=%.3f started=%s\n" % (
    a.nodes, a.analyzer, a.hours, a.cycle_min, t0, datetime.datetime.now().isoformat(timespec="seconds")))
analyzer_sink = None  # a list while a room check runs


def lines_from(name, s):
    partial[name] += s.read(s.in_waiting or 0)
    out = []
    while b"\n" in partial[name]:
        line, partial[name] = partial[name].split(b"\n", 1)
        line = line.decode("ascii", errors="replace").strip()
        if line:
            out.append(line)
    return out


def pump(seconds=0.0):
    """Read every port once (or for `seconds`); node lines into the log, Analyzer lines into analyzer_sink."""
    end = time.time() + seconds
    while True:
        for n, s in ports.items():
            for line in lines_from(n, s):
                log.write("%.2f %s %s\n" % (time.time() - t0, n, line))
        for line in lines_from("AN", analyzer):
            if analyzer_sink is not None:
                analyzer_sink.append(line)
        if time.time() >= end:
            break
        time.sleep(0.005)


def to_nodes(cmd):
    for n, s in ports.items():
        s.write((cmd + "\n").encode())
        log.write("%.2f %s >> %s\n" % (time.time() - t0, n, cmd))
        pump(0.2)


def analyzer_run(cmd, done_on, timeout_s):
    global analyzer_sink
    analyzer_sink = []
    analyzer.write((cmd + "\n").encode())
    start = time.time()
    done_at = None
    while time.time() - start < timeout_s:
        pump(0.5)
        if done_at is None and any(l.startswith(done_on) for l in analyzer_sink):
            done_at = time.time()
        if done_at is not None and time.time() - done_at > 2:
            break
    got, analyzer_sink = analyzer_sink, None
    return [">> " + cmd] + got


def summarize(lines):
    row = {}
    for l in lines:
        if l.startswith("SEQ_SUMMARY"):
            f = dict(re.findall(r"([\w.]+)=(\S+)", l))
            for k in ("expected_trials", "rejected_trials", "miss_trials", "unexpected_trials", "avg_strength"):
                row[k] = f.get(k, "")
    reasons = {}
    for l in lines:
        if l.startswith("SEQ_TRIAL"):
            r = re.search(r"reject_reason=(\w+)", l)
            if r and r.group(1) not in ("none", "result_in_expected_window"):
                reasons[r.group(1)] = reasons.get(r.group(1), 0) + 1
        if l.startswith("SEQ_EXPLAIN"):
            r = re.search(r"failed_label=(\w+)", l)
            if r and r.group(1) != "none":
                reasons["failed_" + r.group(1)] = reasons.get("failed_" + r.group(1), 0) + 1
    row["reasons"] = ";".join("%s:%d" % kv for kv in sorted(reasons.items()))
    return row


cycles_path = os.path.join(a.out, "cycles.csv")
cycles_new = not os.path.exists(cycles_path)
cycles_file = open(cycles_path, "a", newline="", encoding="utf-8")
cyc = csv.writer(cycles_file)
if cycles_new:
    cyc.writerow(["cycle", "local_time", "elapsed_h", "step", "expected", "rejected", "miss", "unexpected",
                  "avg_strength", "reasons"])

# start: the Nodes run free (unsaved), full log
for c in ("PARAM SET output.silent 0", "RB log full"):
    to_nodes(c)

next_status = a.status_every
next_check = a.first_check_min * 60.0
cycle = 0
while time.time() - t0 < a.hours * 3600.0:
    pump(0.05)
    now = time.time() - t0
    if now >= next_status:
        to_nodes("RB STATUS")
        next_status += a.status_every
    if now >= next_check:
        cycle += 1
        log.write("%.2f -- room check %d start\n" % (now, cycle))
        to_nodes("PARAM SET output.silent 1")
        pump(a.settle_s)
        stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
        for name, command in DEFAULT_STEPS:
            lines = analyzer_run(command, "SEQ_SUMMARY", 150)
            with open(os.path.join(a.out, "cycle%02d_%s.log" % (cycle, name)), "w", encoding="utf-8") as f:
                f.write("\n".join(lines) + "\n")
            r = summarize(lines)
            cyc.writerow([cycle, stamp, "%.2f" % ((time.time() - t0) / 3600.0), name, r.get("expected_trials", ""),
                          r.get("rejected_trials", ""), r.get("miss_trials", ""), r.get("unexpected_trials", ""),
                          r.get("avg_strength", ""), r.get("reasons", "")])
            cycles_file.flush()
        for k in range(a.raw_reps):
            lines = analyzer_run(RAW_HOP, "RAW_SUMMARY", 30)
            with open(os.path.join(a.out, "cycle%02d_hop5_r%d.csv" % (cycle, k)), "w", encoding="utf-8") as f:
                f.write("\n".join(l for l in lines if not l.startswith(">> ")) + "\n")
        to_nodes("PARAM SET output.silent 0")
        log.write("%.2f -- room check %d end\n" % (time.time() - t0, cycle))
        log.flush()
        next_check += a.cycle_min * 60.0

to_nodes("PARAM SET output.silent 1")  # leave them quiet; a reboot also boots them silent (stored)
pump(1)
log.close()
cycles_file.close()
print("done: %d room checks, %.2f h; %s" % (cycle, (time.time() - t0) / 3600.0, cycles_path))
