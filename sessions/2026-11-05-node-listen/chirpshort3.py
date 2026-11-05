"""Plays the Node's network signal on the USB Node alone: every window position 250..7900 Hz, then shape variants.
  chirptour.py COM3 LOG"""
import re
import sys
import time

import serial

port, logpath = sys.argv[1], sys.argv[2]
s = serial.Serial()
s.port, s.baudrate, s.timeout = port, 115200, 0.1
s.dtr = False
s.rts = False
s.open()
log = open(logpath, "w", encoding="utf-8")


def cmd(c, wait=0.4):
    s.reset_input_buffer()
    s.write((c + "\n").encode())
    time.sleep(wait)
    r = s.read(16384).decode(errors="replace")
    log.write("%.2f >> %s\n%s\n" % (time.time(), c, r))
    log.flush()
    return r


for c in ["PARAM SET output.silent 1"]:
    cmd("NET * " + c, 2.0)
for c in ["PARAM SET output.silent 0", "PARAM SET output.idle_announce_hz 0", "PARAM SET behavior.band_per_node 1",
          "PARAM SET behavior.base_scatter_bins 0"]:
    cmd(c)


def band():
    m = re.search(r"band=(\d+-\d+)", cmd("RB STATUS", 0.8))
    return m.group(1) if m else "?"


def call(label):
    b = band()
    cmd("RB CALL", 0.2)
    print("%s  %-22s band=%s" % (time.strftime("%H:%M:%S"), label, b), flush=True)
    time.sleep(1.6)


cmd("PARAM SET behavior.band_range_low_hz 2000")
cmd("PARAM SET behavior.band_range_high_hz 3000")
V = [(8, 10, 0), (8, 15, 0), (8, 25, 0), (5, 10, 0), (5, 15, 0), (5, 10, 5), (3, 10, 0), (3, 15, 5)]
import subprocess
subprocess.run(["powershell", "-NoProfile", "-Command", "(New-Object System.Media.SoundPlayer '%s').PlaySync()" % r"C:/Users/admin/AppData/Local/Temp/claude/c----DATA-Nodes-resonantNode/06452682-0eaa-484f-8ae6-05333f655030/scratchpad/cue3.wav"])
time.sleep(0.8)
for n, (steps, pulse, gap) in enumerate(V, 1):
    subprocess.run(["powershell", "-NoProfile", "-Command", "(New-Object System.Media.SoundPlayer '%s').PlaySync()" % (r"C:/Users/admin/AppData/Local/Temp/claude/c----DATA-Nodes-resonantNode/06452682-0eaa-484f-8ae6-05333f655030/scratchpad/beep%d.wav" % n)])
    time.sleep(0.5)
    cmd("PARAM SET output.hop_steps %d" % steps)
    cmd("PARAM SET output.hop_pulse_ms %d" % pulse)
    cmd("PARAM SET output.hop_gap_ms %d" % gap)
    cmd("RB CALL", 0.2)
    time.sleep(0.8)
    cmd("RB CALL", 0.2)
    print("%s  %d: steps %d pulse %d gap %d" % (time.strftime("%H:%M:%S"), n, steps, pulse, gap), flush=True)
    time.sleep(1.4)
for c in ["PARAM SET output.hop_steps 5", "PARAM SET output.hop_pulse_ms 25", "PARAM SET output.hop_gap_ms 10",
          "PARAM SET output.silent 1"]:
    cmd(c)
print("done (E2 and the others silent)", flush=True)
s.close()
