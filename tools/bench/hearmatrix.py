#!/usr/bin/env python3
"""Hearing matrix: each node calls in turn (RB CALL), the others log; listener x caller cells (step 6, 5e).

All boards are opened together (a port open resets a D-AMP board) and set to idle off, behavior.rest_after_field_patterns=1 and a short
rest: a listener that hears a valid pattern rests instead of answering, so no chain forms, and its verdict is still
logged (decision=rest_field_dense / resting, valid=1). Each caller then plays --calls RB CALLs, --every s apart.
A cell counts the caller's calls that the listener logged as a valid verdict within --window s after the caller's
`BEH emit start` line (the PC stamps both, so the window starts slightly before the emit line).

  python tools/bench/hearmatrix.py --boards E1=COM8,E2=COM3,E3=COM4,E4=COM10,E5=COM11 \
      --out bench/sessions/<folder> --name hear_0p3
"""
import argparse, os, re, time
import serial

ap = argparse.ArgumentParser()
ap.add_argument("--boards", required=True)
ap.add_argument("--calls", type=int, default=10)
ap.add_argument("--every", type=float, default=4.0)
ap.add_argument("--rest-ms", type=int, default=2000)
ap.add_argument("--window", type=float, default=1.2)
ap.add_argument("--out", required=True)
ap.add_argument("--name", required=True)
a = ap.parse_args()

boards = [tuple(x.split("=")) for x in a.boards.split(",") if x.strip()]
names = [n for n, _ in boards]
ports = {n: serial.Serial(p, 115200, timeout=0.005) for n, p in boards}
os.makedirs(a.out, exist_ok=True)
log = open(os.path.join(a.out, a.name + ".log"), "w", encoding="utf-8")
t0 = time.time()
lines = []


def pump(secs):
    end = time.time() + secs
    while time.time() < end:
        for n, s in ports.items():
            l = s.readline()
            if l:
                l = l.decode("ascii", "replace").strip()
                t = time.time() - t0
                lines.append((t, n, l))
                log.write("%.2f %s %s\n" % (t, n, l))


pump(7)
for n, s in ports.items():
    # The level automaton off (BEH-008: it acts before the rest rule and would answer a lone call with 1 / L) and the
    # outputs on (the bench boards store output.silent 1), live: a board that reboots comes back as stored.
    for cmd in ("PARAM SET behavior.idle_enabled 0", "PARAM SET behavior.rest_after_field_patterns 1",
                "PARAM SET behavior.rest_ms %d" % a.rest_ms, "PARAM SET behavior.level_max 0",
                "PARAM SET output.silent 0", "PARAM DUMP", "RB log full"):
        s.write((cmd + "\n").encode())
        pump(0.6)
log.write("# setup done: behavior.idle_enabled=0, rest_after_field_patterns=1, rest_ms=%d, level_max=0, "
          "output.silent=0 on every board\n" % a.rest_ms)
pump(a.rest_ms / 1000.0 + 1)
for caller in names:
    for k in range(a.calls):
        ports[caller].write(b"RB CALL\n")
        log.write("%.2f %s >> RB CALL\n" % (time.time() - t0, caller))
        pump(a.every)
for s in ports.values():
    s.close()
log.close()

emits = [(t, n) for t, n, l in lines if l.startswith("BEH emit start")]
verdicts = [(t, n, l) for t, n, l in lines if l.startswith("RB verdict=") and "valid=1" in l]
answers = [(t, n) for t, n, l in lines if l.startswith("BEH emit start") and "source=heard_pattern" in l]
sent = {n: sum(1 for _, e in emits if e == n) for n in names}
cell = {(x, y): 0 for x in names for y in names if x != y}
for te, caller in emits:
    for x in names:
        if x != caller and any(v == x and -0.1 <= t - te <= a.window for t, v, _ in verdicts):
            cell[(x, caller)] += 1
print("%s: calls per node %s; answers (should be 0): %d" % (a.name, " ".join("%s=%d" % (n, sent[n]) for n in names), len(answers)))
print("  listener \ caller " + " ".join("%6s" % y for y in names))
for x in names:
    print("  %-17s " % x + " ".join("%6s" % ("-" if x == y else "%d/%d" % (cell[(x, y)], sent[y])) for y in names))
