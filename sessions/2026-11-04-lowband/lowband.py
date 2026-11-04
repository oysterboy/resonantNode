"""Low-band check: E1 calls a 5-step hop train on a fixed 5-bin band, E2-E5 listen (silent, idle off); per band, how
many listeners log a valid verdict within 1.5 s of each call, and their strengths. Bands: LOW-HIGH Hz list.

  lowband.py OUTDIR 950-1950 500-1500 300-1300
"""
import os
import re
import sys
import threading
import time

import serial

out = sys.argv[1]
bands = sys.argv[2:]
os.makedirs(out, exist_ok=True)
ports = {"E1": "COM8", "E2": "COM3", "E3": "COM4", "E4": "COM10", "E5": "COM11"}
S = {}
for n, p in ports.items():
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = p, 115200, 0.05
    s.dtr = False
    s.rts = False
    s.open()
    S[n] = s
log = open(os.path.join(out, "lowband.log"), "w", encoding="utf-8")
lines = []
stop = False
t0 = time.time()


def reader(n):
    buf = ""
    while not stop:
        d = S[n].read(4096)
        if not d:
            continue
        buf += d.decode(errors="replace")
        while "\n" in buf:
            l, buf = buf.split("\n", 1)
            t = time.time() - t0
            lines.append((t, n, l.strip()))
            log.write("%.2f %s %s\n" % (t, n, l.strip()))
            log.flush()


for n in S:
    threading.Thread(target=reader, args=(n,), daemon=True).start()


def send(n, cmd):
    S[n].write((cmd + "\n").encode())
    log.write("%.2f %s >> %s\n" % (time.time() - t0, n, cmd))
    time.sleep(0.25)


for n in S:
    for c in ("PARAM SET behavior.idle_enabled 0", "PARAM SET behavior.band_per_node 0", "RB log full"):
        send(n, c)
send("E1", "PARAM SET output.silent 0")
results = []
for band in bands:
    lo, hi = band.split("-")
    for n in S:
        send(n, "PARAM SET detection.band_low_hz %s" % lo)
        send(n, "PARAM SET detection.band_high_hz %s" % hi)
    time.sleep(3)
    calls = []
    for k in range(10):
        t = time.time() - t0
        send("E1", "RB CALL")
        calls.append(t)
        time.sleep(8)
    for t in calls:
        heard = {}
        for tt, n, l in lines:
            if n != "E1" and t < tt < t + 1.5 and l.startswith("RB verdict=") and "valid=1" in l:
                m = re.search(r"strength=(\d+)", l)
                heard[n] = int(m.group(1)) if m else 0
        results.append((band, t, heard))
    hops = [l for tt, n, l in lines if n == "E1" and "RB emit shape=hop" in l and tt > calls[0] - 1]
    print(band, "E1 train:", hops[-1][hops[-1].find("order="):] if hops else "none")
    per = [len(h) for b, t, h in results if b == band]
    strengths = [v for b, t, h in results if b == band for v in h.values()]
    print("  listeners hearing each call (of 4):", per, " strengths median %s" % (
        sorted(strengths)[len(strengths) // 2] if strengths else "-"))
send("E1", "PARAM SET output.silent 1")
for n in S:
    send(n, "PARAM LOAD")
time.sleep(2)
stop = True
log.close()
