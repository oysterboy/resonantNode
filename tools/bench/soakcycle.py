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

--at-min 7,27,47 puts the checks at those minutes of the clock instead of every --cycle-min (owner 2026-10-26: hourly
steps in the drift are suspicious, and church bells ring in the distance on the hour and the quarters; a 20 min
grid off the quarters samples the drift three times as densely and never measures while a bell rings).

All ports are opened with DTR/RTS low (no reset) and held for the whole run. A Node that resets mid-run (its
`BUILD role=` line) comes back silent, log minimal, on its stored band: the run logs `-- node reset <NAME>` and
sends it the run's setup again (output.silent as the run has it now, RB log full; review 12.0 L20).

  python tools/bench/soakcycle.py --nodes E1=COM8,E4=COM10,E5=COM11 --analyzer COM3 --hours 24 --cycle-min 60 \\
      --out bench/sessions/<folder> --name soak [--at-min 7,27,47]

Step 14 additions (owner 2026-10-29: the 47 h soak), each opt-in, each per check:
  --steps margin      the room check runs E005's margin set instead of the default three: hop10 at 0.5 / 0.35 /
                      0.25 / 0.21 FS (level=) and hops 2+3 and 4+5 down 20 dB (att=), 10 trials each
  --chord L,A,B       A and B answer the Emitter's hop10 (sent through the Analyzer's EMIT) at 0 / +150 / +250 ms
                      (B's wait raised, the rest rules off, unsaved) while L listens silent; 5 trains each;
                      L's valid verdicts and which of A / B answered, per train, into chord.csv (E003 phase 2)
  --noise-play        bench_audio.wav played once from this PC while every Node listens silent; verdicts per Node
                      into noise.csv (the max duration 450 noise cost, decisions/2026-10-29-max-duration-450...)
"""
import argparse
import csv
import datetime
import os
import re
import subprocess
import time

import serial

HOP = "freq=3300 band=2950-3950 dur=25 pulses=5 gap=10 hop=2950,3200,3450,3700,3950"
DEFAULT_STEPS = [
    ("hop10", "SEQ start profile=TonalPulseScalar tries=10 %s mode=detail when=all verbose=1" % HOP),
    ("chirp3200_100", "SEQ start profile=TonalPulseScalar tries=10 dur=100 band=2950-3950 mode=detail when=all verbose=1"),
    ("chirp3450_150", "SEQ start profile=TonalPulseScalar tries=10 freq=3450 dur=150 band=2950-3950 mode=detail when=all verbose=1"),
]
HOP450 = "freq=3450 band=2950-3950 dur=25 pulses=5 gap=10 hop=2950,3200,3450,3700,3950"
MARGIN_STEPS = [("hop10_L%s" % lv, "SEQ start profile=TonalPulseScalar tries=10 %s level=%s mode=detail when=all "
                 "verbose=1" % (HOP450, lv)) for lv in ("0.5", "0.35", "0.25", "0.21")]
MARGIN_STEPS += [("hop10_att_%s" % n, "SEQ start profile=TonalPulseScalar tries=10 %s att=%s mode=detail when=all "
                  "verbose=1" % (HOP450, at)) for n, at in (("mid23", "0,20,20,0,0"), ("end45", "0,0,0,20,20"))]
AUDIO = r"C:\Users\admin\Downloads\bench_audio.wav"
RAW_HOP = ("RAW trigger f=3300 dur=25 pulses=5 gap=10 hop=2950,3200,3450,3700,3950 pre=50 post=250 mode=both "
           "band=2950-3950")

ap = argparse.ArgumentParser()
ap.add_argument("--nodes", required=True, help="NAME=PORT,...")
ap.add_argument("--analyzer", required=True, help="the Analyzer's port (the Emitter is on its link)")
ap.add_argument("--hours", type=float, default=24.0)
ap.add_argument("--cycle-min", type=float, default=60.0)
ap.add_argument("--first-check-min", type=float, default=2.0, help="the first room check, minutes after the start")
ap.add_argument("--at-min", default="", help="room checks at these minutes of the clock (e.g. 7,27,47); "
                "replaces --cycle-min and --first-check-min")
ap.add_argument("--status-every", type=float, default=120.0)
ap.add_argument("--settle-s", type=float, default=8.0, help="after silencing the Nodes, before the first step")
ap.add_argument("--raw-reps", type=int, default=2)
ap.add_argument("--out", required=True)
ap.add_argument("--name", default="soak")
ap.add_argument("--steps", default="default", choices=("default", "margin"))
ap.add_argument("--chord", default="", help="LISTENER,A,B (node names) for the chord check")
ap.add_argument("--chord-trains", type=int, default=5)
ap.add_argument("--noise-play", action="store_true")
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
node_sink = None  # a list of (time, node, line) while a chord or noise check counts verdicts
silent_now = 1  # what the run last set output.silent to (a reset Node gets it again)


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
                if node_sink is not None:
                    node_sink.append((time.time(), n, line))
                if line.startswith("BUILD role="):  # the boot banner; RB STATUS carries the text too
                    resetup(n, s)
        for line in lines_from("AN", analyzer):
            if analyzer_sink is not None:
                analyzer_sink.append(line)
        if time.time() >= end:
            break
        time.sleep(0.005)


def resetup(n, s):
    """A Node rebooted mid-run: mark it in the log and send it the run's setup again."""
    log.write("%.2f -- node reset %s\n" % (time.time() - t0, n))
    for c in ("PARAM SET output.silent %d" % silent_now, "RB log full"):
        s.write((c + "\n").encode())
        log.write("%.2f %s >> %s\n" % (time.time() - t0, n, c))


def to_node(n, cmd):
    ports[n].write((cmd + "\n").encode())
    log.write("%.2f %s >> %s\n" % (time.time() - t0, n, cmd))
    pump(0.3)


def valid_verdicts(sink, n, t_from, t_to):
    return sum(1 for t, m, l in sink if m == n and t_from <= t < t_to and l.startswith("RB verdict=") and "valid=1" in l)


def answers(sink, n, t_from, t_to):
    return sum(1 for t, m, l in sink if m == n and t_from <= t < t_to and "BEH emit start" in l and
               "source=heard_pattern" in l)


def chord_check(cycle, stamp):
    """A and B answer at 0 / +150 / +250 ms, L listens; per train L's valid verdicts (train + answers) and whether A
    and B answered. The rest rules are off for the check (live): after 10 answers in a minute the own-emit budget
    sent A and B into rest, and one answer could not be told from a merged pair (first run, 2026-10-30)."""
    global node_sink
    L, A, B = a.chord.split(",")
    for c in ("PARAM SET behavior.idle_enabled 0", "PARAM SET behavior.own_emit_budget 0",
              "PARAM SET behavior.rest_after_field_patterns 0"):
        to_nodes(c)
    to_node(L, "PARAM SET output.silent 1")
    to_node(A, "PARAM SET output.silent 0")
    to_node(B, "PARAM SET output.silent 0")
    pump(2)
    rows = []
    for extra in (0, 150, 250):
        to_node(B, "PARAM SET behavior.wait_after_heard_ms %d" % (WAIT0 + extra))
        pump(2)
        counts, both = [], []
        for k in range(a.chord_trains):
            node_sink = []
            t_from = time.time()
            analyzer.write(("EMIT PULSES 5 25 10 trial=%d freq=3450 hop=2950,3200,3450,3700,3950\n" % (k + 1)).encode())
            pump(6)
            t_to = time.time()
            counts.append(valid_verdicts(node_sink, L, t_from, t_to))
            both.append("%d%d" % (min(answers(node_sink, A, t_from, t_to), 1), min(answers(node_sink, B, t_from, t_to), 1)))
            node_sink = None
        rows.append((extra, counts))
        chord_w.writerow([cycle, stamp, "%.2f" % ((time.time() - t0) / 3600.0), extra,
                          " ".join(str(c) for c in counts), " ".join(both)])
    chord_file.flush()
    # Everything back as stored (wait, idle, the rest rules), then the run's own setup.
    to_nodes("PARAM LOAD")
    pump(1)
    to_nodes("PARAM SET output.silent 1")
    to_nodes("RB log full")
    return rows


def noise_check(cycle, stamp):
    """bench_audio.wav once while every Node listens silent; verdicts per Node."""
    global node_sink
    to_nodes("PARAM SET output.silent 1")
    pump(2)
    node_sink = []
    t_from = time.time()
    player = subprocess.Popen(["powershell", "-NoProfile", "-Command",
                               "(New-Object System.Media.SoundPlayer '%s').PlaySync()" % AUDIO])
    while player.poll() is None:
        pump(1)
    pump(3)
    sink, node_sink = node_sink, None
    for n in ports:
        verdicts = sum(1 for t, m, l in sink if m == n and l.startswith("RB verdict="))
        noise_w.writerow([cycle, stamp, "%.2f" % ((time.time() - t0) / 3600.0), n, verdicts,
                          valid_verdicts(sink, n, t_from, time.time())])
    noise_file.flush()


def to_nodes(cmd):
    global silent_now
    if cmd.startswith("PARAM SET output.silent "):
        silent_now = int(cmd.split()[-1])
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
    if done_at is None and cmd.upper().startswith("SEQ START"):
        # No summary within the timeout: stop the run, or the Analyzer refuses the next SEQ start
        # (run_active, 11g C1).
        analyzer.write(b"SEQ STOP\n")
        pump(2)
        analyzer_sink.append(">> SEQ STOP (timeout)")
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


AT_MIN = sorted(int(x) for x in a.at_min.split(",") if x.strip())


def next_clock_check(after_unix):
    """Seconds after t0 of the first clock minute in AT_MIN after after_unix."""
    t = datetime.datetime.fromtimestamp(after_unix).replace(second=0, microsecond=0)
    for _ in range(24 * 60 + 1):
        t += datetime.timedelta(minutes=1)
        if t.minute in AT_MIN:
            return t.timestamp() - t0
    raise ValueError("--at-min: no minute in 0..59")


cycles_path = os.path.join(a.out, "cycles.csv")
cycles_new = not os.path.exists(cycles_path)
cycles_file = open(cycles_path, "a", newline="", encoding="utf-8")
cyc = csv.writer(cycles_file)
if cycles_new:
    cyc.writerow(["cycle", "local_time", "elapsed_h", "step", "expected", "rejected", "miss", "unexpected",
                  "avg_strength", "reasons"])
STEPS = MARGIN_STEPS if a.steps == "margin" else DEFAULT_STEPS


def csv_writer(name, header):
    path = os.path.join(a.out, name)
    new = not os.path.exists(path)
    f = open(path, "a", newline="", encoding="utf-8")
    w = csv.writer(f)
    if new:
        w.writerow(header)
    return f, w


chord_file, chord_w = csv_writer("chord.csv", ["cycle", "local_time", "elapsed_h", "offset_ms", "listener_valid",
                                               "answered_ab"])
noise_file, noise_w = csv_writer("noise.csv", ["cycle", "local_time", "elapsed_h", "node", "verdicts", "valid"])
WAIT0 = 100
if a.chord:
    # B's answer delay as stored, so the chord check's offsets are added to it and it is put back
    b_name = a.chord.split(",")[2]
    ports[b_name].write(b"PARAM GET behavior.wait_after_heard_ms\n")
    got = b""
    end = time.time() + 1.5
    while time.time() < end:
        got += ports[b_name].read(4096)
    found = re.findall(rb"value=(\d+)", got)
    WAIT0 = int(found[-1]) if found else 100

# start: the Nodes run free (unsaved), full log
for c in ("PARAM SET output.silent 0", "RB log full"):
    to_nodes(c)

next_status = a.status_every
next_check = next_clock_check(t0) if AT_MIN else a.first_check_min * 60.0
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
        for name, command in STEPS:
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
        if a.chord:
            chord_check(cycle, stamp)
        if a.noise_play:
            noise_check(cycle, stamp)
        to_nodes("PARAM SET output.silent 0")
        log.write("%.2f -- room check %d end\n" % (time.time() - t0, cycle))
        log.flush()
        next_check = next_clock_check(time.time()) if AT_MIN else next_check + a.cycle_min * 60.0

to_nodes("PARAM SET output.silent 1")  # leave them quiet; a reboot also boots them silent (stored)
pump(1)
log.close()
cycles_file.close()
chord_file.close()
noise_file.close()
print("done: %d room checks, %.2f h; %s" % (cycle, (time.time() - t0) / 3600.0, cycles_path))
