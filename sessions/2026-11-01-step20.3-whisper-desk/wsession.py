"""Whispernet desk session with the owner: phases on a schedule, cue beeps from the PC, every board recording.

  wsession.py --ports E1=COM8,E4=COM10,E5=COM11 --out DIR [--start-in 60] [--dry]

Before: WHISPER GATE flat=1 (the timbre check off, so nothing is filtered) and WHISPER REC 50 on every board. The
schedule (wall clock) goes to DIR/schedule.json and is printed; n quiet beeps 3 s before phase n. The boards' lines
are logged with wall-clock times (DIR/<board>.log). Afterwards: python wseval.py DIR.
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

PHASES = [
    ("whisper_0.5m", 40), ("pause", 20),
    ("whisper_1m", 40), ("pause", 20),
    ("whisper_2m", 40), ("pause", 20),
    ("speech_1m", 30), ("pause", 20),
    ("rustle_typing", 30), ("pause", 20),
]

ap = argparse.ArgumentParser()
ap.add_argument("--ports", required=True)
ap.add_argument("--out", required=True)
ap.add_argument("--start-in", type=float, default=60.0)
ap.add_argument("--dry", action="store_true", help="print the schedule only")
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)

t_start = time.time() + a.start_in
schedule, t = [], t_start
n_active = 0
for name, dur in PHASES:
    cue = 0
    if name != "pause":
        n_active += 1
        cue = n_active
    schedule.append({"phase": name, "start": t, "end": t + dur, "cue_beeps": cue})
    t += dur
json.dump(schedule, open(os.path.join(a.out, "schedule.json"), "w"), indent=1)
for p in schedule:
    if p["phase"] != "pause":
        print("%s-%s  %-14s (%d beep%s before)" % (time.strftime("%H:%M:%S", time.localtime(p["start"])),
                                                  time.strftime("%H:%M:%S", time.localtime(p["end"])),
                                                  p["phase"], p["cue_beeps"], "" if p["cue_beeps"] == 1 else "s"))
if a.dry:
    raise SystemExit

# cue wavs: n beeps of 1 kHz, 80 ms, 200 ms apart, quiet
for n in range(1, len(PHASES) + 1):
    path = os.path.join(a.out, "cue%d.wav" % n)
    rate = 24000
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        frames = b""
        for k in range(n):
            for i in range(int(0.08 * rate)):
                frames += struct.pack("<h", int(1500 * math.sin(2 * math.pi * 1000 * i / rate)))
            frames += b"\x00\x00" * int(0.2 * rate)
        w.writeframes(frames)

boards, stop = {}, False
for kv in a.ports.split(","):
    name, port = kv.split("=")
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.05
    s.dtr = False
    s.rts = False
    s.open()
    boards[name] = {"s": s, "f": open(os.path.join(a.out, name + ".log"), "w", encoding="utf-8"), "buf": ""}


def reader(name):
    b = boards[name]
    while not stop:
        d = b["s"].read(4096)
        if not d:
            continue
        b["buf"] += d.decode(errors="replace")
        while "\n" in b["buf"]:
            line, b["buf"] = b["buf"].split("\n", 1)
            b["f"].write("%.2f %s\n" % (time.time(), line.rstrip()))


for n in boards:
    threading.Thread(target=reader, args=(n,), daemon=True).start()
for c in ("WHISPER GATE flat=1", "WHISPER REC 50", "ECHO LOG 30", "WHISPER STATUS"):
    for b in boards.values():
        b["s"].write((c + "\n").encode())
    time.sleep(0.5)

for p in schedule:
    if p["cue_beeps"]:
        while time.time() < p["start"] - 3:
            time.sleep(0.1)
        cue = os.path.abspath(os.path.join(a.out, "cue%d.wav" % p["cue_beeps"]))
        subprocess.run(["powershell", "-NoProfile", "-Command",
                        "(New-Object System.Media.SoundPlayer '%s').PlaySync()" % cue], check=False)
        print("%s  %s" % (time.strftime("%H:%M:%S"), p["phase"]), flush=True)
    while time.time() < p["end"]:
        time.sleep(0.1)
time.sleep(10)  # the last dumps
for b in boards.values():
    b["s"].write(b"WHISPER STATUS\n")
time.sleep(2)
stop = True
time.sleep(0.3)
for b in boards.values():
    b["f"].close()
print("done")
