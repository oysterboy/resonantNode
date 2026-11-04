"""A desk session on a schedule: sections with cue beeps from the lab PC, commands sent to the boards at each
section's start, every board's lines logged with wall-clock times, a status asked at each section's end.

  python tools/bench/deskrun.py <plan.json> --out DIR [--start-in 60] [--dry]

plan.json:
  {"boards": {"E5": "COM11"},
   "setup": ["PARAM SET whisper.aec 1"],                 # once, before the first section
   "status": ["WHISPER STATUS"],                         # at each section's end
   "sections": [
     {"name": "whisper_short_deaf", "secs": 60, "cue": true, "cmds": ["PARAM SET whisper.listen_while_playing 0"],
      "say": "whisper short sentences, 0.5-1 m, pauses between"},
     {"name": "pause", "secs": 20}]}

A section with "cue" gets n quiet beeps 3 s before it (n counts the cued sections). The schedule (wall clock) goes
to DIR/schedule.json and is printed with each section's "say". Evaluate with deskeval.py DIR. The ports open
without resetting the boards (DTR / RTS low) and close at the end.
"""
import argparse
import json
import math
import os
import struct
import subprocess
import threading
import time
import wave

import serial

ap = argparse.ArgumentParser()
ap.add_argument("plan")
ap.add_argument("--out", required=True)
ap.add_argument("--start-in", type=float, default=60.0)
ap.add_argument("--dry", action="store_true")
a = ap.parse_args()
plan = json.load(open(a.plan, encoding="utf-8"))
os.makedirs(a.out, exist_ok=True)

t = time.time() + a.start_in
schedule, cued = [], 0
for sec in plan["sections"]:
    cue = 0
    if sec.get("cue"):
        cued += 1
        cue = cued
    schedule.append(dict(sec, start=t, end=t + sec["secs"], cue_beeps=cue))
    t += sec["secs"]
json.dump(schedule, open(os.path.join(a.out, "schedule.json"), "w", encoding="utf-8"), indent=1)
for s in schedule:
    if s.get("say") or s["cue_beeps"]:
        print("%s-%s  %-24s %s%s" % (time.strftime("%H:%M:%S", time.localtime(s["start"])),
                                     time.strftime("%H:%M:%S", time.localtime(s["end"])), s["name"],
                                     ("(%d beeps) " % s["cue_beeps"]) if s["cue_beeps"] else "", s.get("say", "")))
if a.dry:
    raise SystemExit

for n in range(1, cued + 1):
    path = os.path.join(a.out, "cue%d.wav" % n)
    rate = 24000
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        frames = b""
        for k in range(n):
            frames += b"".join(struct.pack("<h", int(1500 * math.sin(2 * math.pi * 1000 * i / rate)))
                               for i in range(int(0.08 * rate)))
            frames += b"\x00\x00" * int(0.2 * rate)
        w.writeframes(frames)

boards, stop = {}, False
for name, port in plan["boards"].items():
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.05
    s.dtr = False
    s.rts = False
    s.open()
    boards[name] = {"s": s, "f": open(os.path.join(a.out, name + ".log"), "w", encoding="utf-8"), "buf": ""}


def reader(name):
    b = boards[name]
    while not stop:
        try:
            d = b["s"].read(4096)
        except serial.SerialException:
            time.sleep(1)
            continue
        if not d:
            continue
        b["buf"] += d.decode(errors="replace")
        while "\n" in b["buf"]:
            line, b["buf"] = b["buf"].split("\n", 1)
            b["f"].write("%.2f %s\n" % (time.time(), line.rstrip()))
            b["f"].flush()


def send(cmd):
    for b in boards.values():
        b["f"].write("%.2f >> %s\n" % (time.time(), cmd))
        b["s"].write((cmd + "\n").encode())
    time.sleep(0.3)


for n in boards:
    threading.Thread(target=reader, args=(n,), daemon=True).start()
for c in plan.get("setup", []):
    send(c)
for s in schedule:
    if s["cue_beeps"]:
        while time.time() < s["start"] - 3:
            time.sleep(0.1)
        cue = os.path.abspath(os.path.join(a.out, "cue%d.wav" % s["cue_beeps"]))
        subprocess.run(["powershell", "-NoProfile", "-Command",
                        "(New-Object System.Media.SoundPlayer '%s').PlaySync()" % cue], check=False)
    while time.time() < s["start"]:
        time.sleep(0.1)
    for c in s.get("cmds", []):
        send(c)
    print("%s  %s" % (time.strftime("%H:%M:%S"), s["name"]), flush=True)
    while time.time() < s["end"]:
        time.sleep(0.1)
    for c in plan.get("status", []):
        send(c)
time.sleep(3)
stop = True
time.sleep(0.3)
for b in boards.values():
    b["f"].close()
    b["s"].close()
print("done")
