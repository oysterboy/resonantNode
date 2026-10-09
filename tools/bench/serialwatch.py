"""Watch one or more boards' serial consoles at once, optionally reset them
and send timed commands; one timestamped line per console line.

  python tools/bench/serialwatch.py --port NODE=COM10 --port EMIT=COM6 \\
      --reset NODE --reset EMIT --seconds 110 \\
      --send "NODE@1=RB log full" --send "NODE@108=RB summary" --out node.log

--port NAME=COMx (repeatable), --reset NAME pulses RTS (EN) on that board at
the start, --send NAME@T=COMMAND writes COMMAND to NAME at T seconds.
Lines are printed (and written to --out) as "<t> NAME | <line>"; non-ASCII
bytes are escaped, so a junk byte on a UART shows up instead of vanishing.
For SEQ runs use seqrun.py (it tags runs with firmware and writes the
session); this is for Node/Emitter consoles and link checks.
"""
import argparse
import sys
import time

import serial

ap = argparse.ArgumentParser()
ap.add_argument("--port", action="append", required=True, metavar="NAME=COMx")
ap.add_argument("--baud", type=int, default=115200)
ap.add_argument("--reset", action="append", default=[], metavar="NAME")
ap.add_argument("--send", action="append", default=[], metavar="NAME@T=COMMAND")
ap.add_argument("--seconds", type=float, default=10.0)
ap.add_argument("--out")
args = ap.parse_args()

ports = {}
for spec in args.port:
    name, dev = spec.split("=", 1)
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = dev, args.baud, 0
    s.dtr = False
    s.rts = False
    s.open()
    ports[name] = s

sends = []
for spec in args.send:
    target, command = spec.split("=", 1)
    name, at = target.split("@", 1)
    sends.append((float(at), name, command))
sends.sort()

out = open(args.out, "w", encoding="utf-8") if args.out else None


def emit(text):
    print(text, flush=True)
    if out:
        out.write(text + "\n")


for name in args.reset:
    ports[name].rts = True
time.sleep(0.1)
for name in args.reset:
    ports[name].rts = False

t0 = time.time()
bufs = {name: b"" for name in ports}
while time.time() - t0 < args.seconds:
    now = time.time() - t0
    while sends and sends[0][0] <= now:
        _, name, command = sends.pop(0)
        ports[name].write((command + "\n").encode())
        emit(f"{now:7.2f} >>>> {name}: {command}")
    for name, s in ports.items():
        bufs[name] += s.read(65536)
        while b"\n" in bufs[name]:
            line, bufs[name] = bufs[name].split(b"\n", 1)
            text = line.rstrip(b"\r").decode("ascii", "backslashreplace")
            if text:
                emit(f"{time.time() - t0:7.2f} {name} | {text}")
    time.sleep(0.01)

for s in ports.values():
    s.close()
if out:
    out.close()
