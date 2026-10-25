"""Which action drops audio DMA buffers? Read i2s.dropped_dma_buffers on all five before and after each step."""
import re
import time

import serial

PORTS = {"E1": "COM8", "E2": "COM3", "E3": "COM4", "E4": "COM10", "E5": "COM11"}
ports = {}
for n, p in PORTS.items():
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = p, 115200, 0.02
    s.dtr = False
    s.rts = False
    s.open()
    ports[n] = s
buf = {n: "" for n in PORTS}


def pump(sec):
    end = time.time() + sec
    while time.time() < end:
        for n, s in ports.items():
            d = s.read(8192)
            if d:
                buf[n] += d.decode(errors="replace")


def drops():
    m = {n: len(buf[n]) for n in PORTS}
    for s in ports.values():
        s.write(b"RB STATUS\n")
    pump(1.5)
    out = {}
    for n in PORTS:
        r = re.findall(r"i2s\.dropped_dma_buffers=(\d+)", buf[n][m[n]:])
        out[n] = int(r[-1]) if r else None
    return out


def step(name, action, wait):
    before = drops()
    action()
    pump(wait)
    after = drops()
    print("%-42s %s" % (name, " ".join("%s:+%s" % (n, (after[n] - before[n]) if None not in (after[n], before[n]) else "?") for n in PORTS)), flush=True)


def gw(cmd):
    return lambda: ports["E1"].write((cmd + "\n").encode())


pump(3)
step("RB STATUS only (the probe itself)", lambda: None, 1)
step("idle 120 s, radio on, no traffic", lambda: None, 120)
step("serial PARAM DUMP on every node", lambda: [s.write(b"PARAM DUMP\n") for s in ports.values()], 3)
step("serial PARAM SAVE on E2 (NVS write)", lambda: ports["E2"].write(b"PARAM SAVE\n"), 3)
step("radio NET * NET PING", gw("NET * NET PING"), 3)
step("radio NET * PARAM GET output.silent", gw("NET * PARAM GET output.silent"), 3)
step("radio NET * PARAM DUMP", gw("NET * PARAM DUMP"), 6)
step("radio NET * PARAM SET behavior.rest_ms 5000", gw("NET * PARAM SET behavior.rest_ms 5000"), 3)
step("radio NET * PARAM SAVE (NVS write)", gw("NET * PARAM SAVE"), 4)
m = len(buf["E1"])
ports["E1"].write(b"NET * PARAM DUMP\n")
pump(6)
parts = {}
for l in buf["E1"][m:].splitlines():
    r = re.match(r"NET reply from=(\S+) seq=\d+ part=(\d+) last=(\d)", l.strip())
    if r:
        parts.setdefault(r.group(1), set()).add(int(r.group(2)))
print("DUMP by radio, frames per node:", {k: (len(v), sorted(v) == list(range(len(v)))) for k, v in sorted(parts.items())})
for s in ports.values():
    s.close()
