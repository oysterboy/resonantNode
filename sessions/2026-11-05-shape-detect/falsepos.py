"""False triggers: two silent Nodes hear the same noise file, A at the current detection, B at a lower minimum;
RB verdict lines (any, valid=1) per section of bench_audio.wav.
  falsepos.py PORT_A PORT_B LOG"""
import subprocess
import sys
import threading
import time

import serial

pa, pb, logpath = sys.argv[1], sys.argv[2], sys.argv[3]
WAV = r"C:\Users\admin\Downloads\bench_audio.wav"
SECTIONS = [("silence", 0, 10), ("drum_bass", 10, 110), ("female", 115, 215), ("male", 220, 320),
            ("flute", 325, 425), ("silence_end", 425, 435)]
log = open(logpath, "w", encoding="utf-8")
ports, lines, stop = {}, [], False
for name, port in (("A", pa), ("B", pb)):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.05
    s.dtr = False
    s.rts = False
    s.open()
    ports[name] = s


def reader(name):
    buf = ""
    while not stop:
        d = ports[name].read(8192)
        if not d:
            continue
        buf += d.decode(errors="replace")
        while "\n" in buf:
            ln, buf = buf.split("\n", 1)
            t = time.time()
            lines.append((t, name, ln.rstrip()))
            log.write("%.3f %s %s\n" % (t, name, ln.rstrip()))


for n in ports:
    threading.Thread(target=reader, args=(n,), daemon=True).start()


def send(name, c):
    ports[name].write((c + "\n").encode())
    log.write("%.3f %s >> %s\n" % (time.time(), name, c))
    time.sleep(0.35)


common = ["PARAM SET output.silent 1", "PARAM SET behavior.band_per_node 1", "PARAM SET behavior.band_range_low_hz 2000",
          "PARAM SET behavior.band_range_high_hz 3000", "RB log full"]
for n in ports:
    for c in common:
        send(n, c)
send("A", "PARAM SET detection.scalar_min_duration_ms 92")
send("A", "PARAM SET detection.scalar_min_release_coverage_ms 80")
send("B", "PARAM SET detection.scalar_min_duration_ms 70")
send("B", "PARAM SET detection.scalar_min_release_coverage_ms 60")
time.sleep(2)
t0 = time.time()
subprocess.run(["powershell", "-NoProfile", "-Command", "(New-Object System.Media.SoundPlayer '%s').PlaySync()" % WAV])
time.sleep(2)
send("B", "PARAM SET detection.scalar_min_duration_ms 92")
send("B", "PARAM SET detection.scalar_min_release_coverage_ms 80")
for n in ports:
    send(n, "RB log minimal")
stop = True
time.sleep(0.3)
print("section       A(92/80) verdicts valid   B(70/60) verdicts valid")
for sec, a, b in SECTIONS:
    row = []
    for n in ("A", "B"):
        v = [ln for t, nn, ln in lines if nn == n and t0 + a <= t < t0 + b and ln.startswith("RB verdict=")]
        row += [len(v), sum(" valid=1" in x for x in v)]
    print("%-12s  %8d %5d           %8d %5d" % (sec, *row))
log.close()
for s in ports.values():
    s.close()
