"""R1's SELFTEST with the light's effects running and the other nodes muted (the light rises to full with each own
chirp: the worst case for the light against the amp check). E1-E3 muted by radio through the gateway first, then
R1's port opened (restarts it, so the light is back on its effects), three SELFTESTs with the idle call off, RB STATUS
before and after; the others unmuted and the call back on at the end.

  python quiet_selftest.py COM3 COM12
"""
import re
import sys
import time

import serial

r1_port, gw_port = sys.argv[1], sys.argv[2]
OTHERS = ("E1", "E2", "E3")
KEYS = ("i2s.dropped_dma_buffers", "i2s.overflows", "audio.block_us_max", "net.tx_failed")
log = open("quiet_selftest.log", "w", encoding="utf-8")


def open_port(name):
    s = serial.Serial(name, 115200, timeout=0.2)
    s.dtr = False
    s.rts = False
    return s


def cmd(s, line, until, timeout=8.0):
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
    return {k: float(m.group(1)) for k in KEYS for m in [re.search(re.escape(k) + r"=(-?[\d.]+)", text)] if m}


gw = open_port(gw_port)
time.sleep(3)
gw.read(65536)
for name in OTHERS:
    t = cmd(gw, "NET %s PARAM SET output.silent 1" % name, r"NET %s .*(status=|done)" % name, timeout=6)
    print(name, "muted" if "status=ok" in t else "no ack: " + t.strip()[-80:], flush=True)
try:
    r1 = open_port(r1_port)
    end = time.time() + 30
    boot = ""
    while time.time() < end and "RB baseline active" not in boot:
        boot += r1.read(4096).decode("utf-8", "replace")
    log.write(boot)
    print("boot active" if "RB baseline active" in boot else "boot: no quiet", flush=True)
    cmd(r1, "PARAM SET behavior.call_enabled 0", "status=")
    before = keys(cmd(r1, "RB STATUS", "BUILD role"))
    for i in range(3):
        t = cmd(r1, "SELFTEST", "SELFTEST_SUMMARY", timeout=45)
        amp = re.search(r"check=amp result=(\w+).*?floor=(\d+) snr_db=([\d.]+) snr_min_db=([\d.]+) snr_max_db=([\d.]+) "
                        r"tone_spread=([\d.]+)", t)
        own = re.search(r"check=own_chirp_suppressed result=(\w+)", t)
        print("run", i + 1, "amp", amp.groups() if amp else None, "own_chirp", own.group(1) if own else None,
              flush=True)
    after = keys(cmd(r1, "RB STATUS", "BUILD role"))
    print("delta", {k: after[k] - before[k] for k in before if k in after}, "block_us_max", after.get(
        "audio.block_us_max"), flush=True)
    cmd(r1, "PARAM SET behavior.call_enabled 1", "status=")
finally:
    for name in OTHERS:
        t = cmd(gw, "NET %s PARAM SET output.silent 0" % name, r"NET %s .*(status=|done)" % name, timeout=6)
        print(name, "unmuted" if "status=ok" in t else "no ack: " + t.strip()[-80:], flush=True)
    log.close()
