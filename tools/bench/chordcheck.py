"""Chord bench cell (step 14, docs/research/polyphony.md Part A check 3): two Nodes answer the Emitter's hop train
together while a third, silent Node listens; per train, how many valid verdicts does the listener log, and when.

The answering Nodes' offset is set through behavior.wait_after_heard_ms (live, not saved): the second answers
--extra-ms later. Cells: each answerer alone, both at the same wait, then both with each --offsets value.

  python tools/bench/chordcheck.py --listener E1=COM8 --a E4=COM10 --b E5=COM11 --emitter COM4 \
      --out bench/sessions/<folder> [--trains 15] [--offsets 50,150,250]

Leaves the Nodes as found: PARAM LOAD (silent, idle and wait back to what is stored) and the RB log mode put back.
"""
import argparse
import os
import re
import time

import serial

ap = argparse.ArgumentParser()
ap.add_argument("--listener", required=True)
ap.add_argument("--a", required=True)
ap.add_argument("--b", required=True)
ap.add_argument("--emitter", required=True)
ap.add_argument("--out", required=True)
ap.add_argument("--trains", type=int, default=15)
ap.add_argument("--period", type=float, default=6.0, help="seconds between Emitter trains")
ap.add_argument("--offsets", default="50,150,250")
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)
HOP = "EMIT PULSES 5 25 10 trial=%d freq=3450 hop=2950,3200,3450,3700,3950"


def op(spec):
    name, port = spec.split("=")
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.02
    s.dtr = False
    s.rts = False
    s.open()
    return name, s


nodes = dict(op(x) for x in (a.listener, a.a, a.b))
L, A, B = (x.split("=")[0] for x in (a.listener, a.a, a.b))
em = serial.Serial()
em.port, em.baudrate, em.timeout = a.emitter, 115200, 0.02
em.dtr = False
em.rts = False
em.open()
log = {n: [] for n in nodes}  # (host time, line)
part = {n: "" for n in nodes}


def pump(sec):
    end = time.time() + sec
    while time.time() < end:
        for n, s in nodes.items():
            d = s.read(8192)
            if d:
                part[n] += d.decode(errors="replace")
                *lines, part[n] = part[n].split("\n")
                now = time.time()
                for line in lines:
                    log[n].append((now, line.rstrip("\r")))
        em.read(8192)


def cmd(n, line, sec=0.6):
    nodes[n].write((line + "\n").encode())
    pump(sec)


def lines_since(n, t0, t1, pattern):
    return [(t, l) for t, l in log[n] if t0 <= t < t1 and re.search(pattern, l)]


pump(0.5)
modes = {}
for n in nodes:
    m0 = len(log[n])
    cmd(n, "RB log", 0.6)
    found = [re.search(r"RB log mode=(\w+)", l) for _, l in log[n][m0:]]
    modes[n] = next((m.group(1) for m in found if m), "minimal")
    cmd(n, "RB log full", 0.4)
    cmd(n, "PARAM SET behavior.idle_enabled 0", 0.4)
    # The level automaton (BEH-008) off: A and B answer every train, so the cell measures what a chord does to
    # detection, not how often the automaton answers.
    cmd(n, "PARAM SET behavior.level_max 0", 0.4)
cmd(L, "PARAM SET output.silent 1", 0.4)  # the listener never answers (review 14.1)
cmd(A, "PARAM GET behavior.wait_after_heard_ms", 0.6)
wait0 = int(re.findall(r"value=(\d+)", " ".join(l for _, l in log[A][-5:]))[-1])

cells = [("A_alone", (A,), 0), ("B_alone", (B,), 0), ("both_same", (A, B), 0)]
cells += [("both_plus%d" % int(o), (A, B), int(o)) for o in a.offsets.split(",")]
results = []
trial = 0


def restore():
    for n in nodes:
        cmd(n, "PARAM LOAD", 1.2)
        cmd(n, "RB log " + modes[n], 0.4)


try:
    for name, answering, extra in cells:
        for n in (A, B):
            cmd(n, "PARAM SET output.silent %d" % (0 if n in answering else 1), 0.4)
        cmd(A, "PARAM SET behavior.wait_after_heard_ms %d" % wait0, 0.4)
        cmd(B, "PARAM SET behavior.wait_after_heard_ms %d" % (wait0 + extra), 0.4)
        pump(2.0)
        counts = []
        for _ in range(a.trains):
            trial += 1
            t0 = time.time()
            em.write((HOP % trial + "\n").encode())
            pump(a.period)
            verdicts = lines_since(L, t0, t0 + a.period, r"^RB verdict=.*valid=1")
            emits = {n: len(lines_since(n, t0, t0 + a.period, r"BEH emit start .*source=heard_pattern")) for n in (A, B)}
            offs = [round((t - t0) * 1000) for t, _ in verdicts]
            counts.append((len(verdicts), emits[A], emits[B], offs))
        results.append((name, extra, counts))
        hist = {}
        for c in counts:
            hist[c[0]] = hist.get(c[0], 0) + 1
        print("%-14s +%3d ms  listener valid verdicts per train %s  answers %s=%d %s=%d" % (
            name, extra, dict(sorted(hist.items())), A, sum(c[1] for c in counts), B, sum(c[2] for c in counts)),
            flush=True)
finally:
    # PARAM LOAD puts back what is stored; a value that was never PARAM SAVEd stays live until a reboot.
    restore()

for n in nodes:
    with open(os.path.join(a.out, "%s.log" % n), "w", encoding="utf-8") as f:
        for t, l in log[n]:
            f.write("%.3f %s\n" % (t, l))
with open(os.path.join(a.out, "results.csv"), "w", encoding="utf-8") as f:
    f.write("cell,extra_ms,train,listener_valid,answers_a,answers_b,verdict_ms\n")
    for name, extra, counts in results:
        for i, (v, ea, eb, offs) in enumerate(counts):
            f.write("%s,%d,%d,%d,%d,%d,%s\n" % (name, extra, i + 1, v, ea, eb, " ".join(map(str, offs))))
