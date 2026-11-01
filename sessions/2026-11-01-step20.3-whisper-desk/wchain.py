"""Whispernet with several boards: log every board's WHISPER lines while the PC plays files, then a silent watch.

  wchain.py --ports E1=COM8,E4=COM10,E5=COM11 --out DIR --files a.wav,b.wav [--watch-s 60]
"""
import argparse
import os
import re
import subprocess
import threading
import time

import serial

ap = argparse.ArgumentParser()
ap.add_argument("--ports", required=True)
ap.add_argument("--out", required=True)
ap.add_argument("--files", default="")
ap.add_argument("--watch-s", type=float, default=60.0)
ap.add_argument("--cmd", default="")
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)
t0 = time.time()
boards = {}
for kv in a.ports.split(","):
    name, port = kv.split("=")
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.05
    s.dtr = False
    s.rts = False
    s.open()
    boards[name] = {"s": s, "lines": [], "buf": ""}
stop = False


def reader(name):
    b = boards[name]
    while not stop:
        d = b["s"].read(4096)
        if not d:
            continue
        b["buf"] += d.decode(errors="replace")
        while "\n" in b["buf"]:
            line, b["buf"] = b["buf"].split("\n", 1)
            b["lines"].append((time.time() - t0, line.rstrip()))


threads = [threading.Thread(target=reader, args=(n,), daemon=True) for n in boards]
for t in threads:
    t.start()


def send(text):
    for b in boards.values():
        b["s"].write((text + "\n").encode())


for c in filter(None, a.cmd.split(";")):
    send(c.strip())
    time.sleep(0.3)
send("ECHO LOG 10")
send("WHISPER STATUS")
time.sleep(3)
marks = []
for f in filter(None, a.files.split(",")):
    marks.append((time.time() - t0, "play " + os.path.basename(f)))
    subprocess.run(["powershell", "-NoProfile", "-Command",
                    "(New-Object System.Media.SoundPlayer '%s').PlaySync()" % os.path.abspath(f)], check=False)
    marks.append((time.time() - t0, "end " + os.path.basename(f)))
    time.sleep(3)
marks.append((time.time() - t0, "watch"))
time.sleep(a.watch_s)
send("WHISPER STATUS")
time.sleep(2)
stop = True
events = []
for n, b in boards.items():
    open(os.path.join(a.out, n + ".log"), "w", encoding="utf-8").write(
        "\n".join("%.2f %s" % (t, l) for t, l in b["lines"]))
    for t, l in b["lines"]:
        if l.startswith("WHISPER heard"):
            events.append((t, n, l[14:]))
for t, m in marks:
    events.append((t, "--", m))
for t, n, l in sorted(events):
    print("%7.2f %-3s %s" % (t, n, l[:150]))
for n, b in boards.items():
    st = [l for _, l in b["lines"] if l.startswith("WHISPER status")]
    if st:
        print(n, st[-1][15:200])
