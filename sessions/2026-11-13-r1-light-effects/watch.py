"""R1 with the light's effects (b8d388e6 + e9212ad9): boot, then the light following Behavior for a while, with the
audio counters before and after (does 50 frames a second cost the audio loop?), the LIGHT hsb / auto commands and
two SELFTESTs with the effects running. Opening COM3 restarts R1.

  python watch.py COM3 240
"""
import re
import sys
import time

import serial

KEYS = ("i2s.dropped_dma_buffers", "i2s.overflows", "i2s.read_errors", "audio.block_us_max", "audio.us_per_sample",
        "heap.free_min", "heard.valid", "output.started", "net.tx_failed")

port, listen_s = sys.argv[1], int(sys.argv[2])
log = open("console.log", "w", encoding="utf-8")
s = serial.Serial(port, 115200, timeout=0.2)
s.dtr = False
s.rts = False


def read_for(seconds):
    end = time.time() + seconds
    text = ""
    while time.time() < end:
        text += s.read(4096).decode("utf-8", "replace")
    log.write(text)
    log.flush()
    return text


def cmd(line, until, timeout=8.0):
    s.reset_input_buffer()
    s.write((line + "\n").encode())
    log.write(">> " + line + "\n")
    end = time.time() + timeout
    text = ""
    while time.time() < end:
        text += s.read(4096).decode("utf-8", "replace")
        if re.search(until, text):
            time.sleep(0.4)
            text += s.read(4096).decode("utf-8", "replace")
            break
    log.write(text)
    log.flush()
    return text


def keys(text):
    out = {}
    for k in KEYS:
        m = re.search(re.escape(k) + r"=(-?[\d.]+)", text)
        if m:
            out[k] = float(m.group(1))
    return out


boot = read_for(12)
for pat in (r"BUILD role=\S+ git=\S+", r"PARAM load stored=\S+.*", r"RB light .*", r"PARAM refused.*"):
    m = re.search(pat, boot)
    print(m.group(0) if m else "(no %s)" % pat, flush=True)

print(cmd("PARAM GET light.max_ma", "light.max_ma").strip().splitlines()[-1], flush=True)
cmd("RB log full", "RB log")
before = keys(cmd("RB STATUS", "BUILD role"))
print("before", before, flush=True)
text = read_for(listen_s)
after = keys(cmd("RB STATUS", "BUILD role"))
print("after ", after, flush=True)
print("delta", {k: after[k] - before[k] for k in before if k in after}, flush=True)
print("verdicts", len(re.findall(r"RB verdict=", text)), "emits", len(re.findall(r"BEH emit start", text)), flush=True)

for line, until in (("LIGHT hsb 120 100 100", "LIGHT "), ("LIGHT hsb 0 0 50", "LIGHT "), ("LIGHT auto", "LIGHT ")):
    print(cmd(line, until).strip().splitlines()[-1], flush=True)
    time.sleep(1.5)

cmd("PARAM SET behavior.call_enabled 0", "status=")
for i in range(2):
    t = cmd("SELFTEST", "SELFTEST_SUMMARY", timeout=45)
    m = re.search(r"SELFTEST_SUMMARY.*", t)
    print(m.group(0) if m else "no summary", flush=True)
cmd("PARAM SET behavior.call_enabled 1", "status=")
cmd("RB log minimal", "RB log")
log.close()
