"""Does a listening Node hear each signal shape? E2 (player) plays every shape n times; E3 (listener, silent) logs
its RB verdict lines; per shape and pass: calls, verdicts seen, valid=1. Pass A: E3's detection as is; pass B:
scalar_min_duration_ms 40 and scalar_min_release_coverage_ms 30 on E3, live (restored after).
  shapedetect.py PLAYER_PORT LISTENER_PORT LOG [n]"""
import re
import sys
import threading
import time

import serial

pp, lp, logpath = sys.argv[1], sys.argv[2], sys.argv[3]
N = int(sys.argv[4]) if len(sys.argv) > 4 else 10
SHAPES = [("5", 5, 15, 0)]


def opened(port):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.05
    s.dtr = False
    s.rts = False
    s.open()
    return s


P, L = opened(pp), opened(lp)
log = open(logpath, "w", encoding="utf-8")
lines, stop = [], False


def reader():
    buf = ""
    while not stop:
        d = L.read(8192)
        if not d:
            continue
        buf += d.decode(errors="replace")
        while "\n" in buf:
            ln, buf = buf.split("\n", 1)
            t = time.time()
            lines.append((t, ln.rstrip()))
            log.write("%.3f L %s\n" % (t, ln.rstrip()))


threading.Thread(target=reader, daemon=True).start()


def send(s, c, wait=0.35):
    s.write((c + "\n").encode())
    log.write("%.3f %s >> %s\n" % (time.time(), "P" if s is P else "L", c))
    time.sleep(wait)


for s in (P, L):
    for c in ["PARAM SET behavior.band_per_node 1", "PARAM SET behavior.band_range_low_hz 2000",
              "PARAM SET behavior.band_range_high_hz 3000", "PARAM SET output.idle_announce_hz 0"]:
        send(s, c)
send(P, "PARAM SET output.silent 0")
send(P, "PARAM SET behavior.level_max 0")  # the level automaton off: no rest after n emissions
send(L, "PARAM SET output.silent 1")
send(L, "RB log full")
time.sleep(2)
results = []
for name_pass, extra, undo in [("92/80", ["PARAM SET detection.scalar_min_duration_ms 92", "PARAM SET detection.scalar_min_release_coverage_ms 80"], []), ("70/80", ["PARAM SET detection.scalar_min_duration_ms 70", "PARAM SET detection.scalar_min_release_coverage_ms 80"], []), ("70/60", ["PARAM SET detection.scalar_min_duration_ms 70", "PARAM SET detection.scalar_min_release_coverage_ms 60"], ["PARAM SET detection.scalar_min_duration_ms 92", "PARAM SET detection.scalar_min_release_coverage_ms 80"])]:
    for c in extra:
        send(L, c)
    for name, steps, pulse, gap in SHAPES:
        send(P, "PARAM SET output.hop_steps %d" % steps)
        send(P, "PARAM SET output.hop_pulse_ms %d" % pulse)
        send(P, "PARAM SET output.hop_gap_ms %d" % gap)
        heard = valid = 0
        durs = []
        for _ in range(N):
            t0 = time.time()
            send(P, "RB CALL", 1.4)
            got = [ln for t, ln in lines if t0 < t < t0 + 1.4 and ln.startswith("RB verdict=")]
            if got:
                heard += 1
                if any(" valid=1" in g for g in got):
                    valid += 1
                m = re.search(r"duration_ms=(\d+)", got[0])
                if m:
                    durs.append(int(m.group(1)))
        r = (name_pass, name, steps, pulse, gap, heard, valid, sorted(durs)[len(durs) // 2] if durs else 0)
        results.append(r)
        print("%s shape %-3s %d x %3d ms gap %2d: verdicts %2d/%d valid %2d/%d  dur_ms~%d" %
              (r[0], r[1], r[2], r[3], r[4], r[5], N, r[6], N, r[7]), flush=True)
    for c in undo:
        send(L, c)
for c in ["PARAM SET behavior.level_max 6", "PARAM SET output.hop_steps 5", "PARAM SET output.hop_pulse_ms 25", "PARAM SET output.hop_gap_ms 10",
          "PARAM SET output.silent 1"]:
    send(P, c)
send(L, "RB log minimal")
time.sleep(1)
stop = True
time.sleep(0.2)
log.close()
P.close()
L.close()
