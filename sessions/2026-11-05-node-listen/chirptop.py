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


print("A: positions top down, 5 pulses x 25 ms, gap 10", flush=True)
for lo in range(6750, 3249, -250):
    cmd("PARAM SET behavior.band_range_high_hz %d" % (lo + 1000))
    cmd("PARAM SET behavior.band_range_low_hz %d" % lo)
    call("pos %d-%d" % (lo, lo + 1000))
for c in ["PARAM SET output.silent 1"]:
    cmd(c)
print("done (E2 and the others silent)", flush=True)
s.close()
