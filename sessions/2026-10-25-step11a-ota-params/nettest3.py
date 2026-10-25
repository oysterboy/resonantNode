"""Step 11a, all commands by radio: RB STATUS (one ~2 KB line), a paced DUMP, SELFTEST rules; drops per step."""
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
    return {n: int((re.findall(r"i2s\.dropped_dma_buffers=(\d+)", buf[n][m[n]:]) or ["-1"])[-1]) for n in PORTS}


def gw(cmd, wait):
    before = drops()
    m = len(buf["E1"])
    ports["E1"].write((cmd + "\n").encode())
    pump(wait)
    out = buf["E1"][m:]
    after = drops()
    d = " ".join("%s:+%d" % (n, after[n] - before[n]) for n in PORTS)
    return out, d


pump(8)
print("== NET * RB STATUS: one ~2 KB line per node, joined at the gateway")
out, d = gw("NET * RB STATUS", 6)
for l in out.splitlines():
    r = re.match(r"NET reply from=(E\d) seq=\d+ part=(\d+) (RB STATUS.*)", l.strip())
    if r:
        print("   %s: %d chars, ends with board=: %s, parts %s" % (r.group(1), len(r.group(3)), "board=" in r.group(3)[-40:], r.group(2)))
print("   drops:", d)

print("== NET * PARAM DUMP, paced")
out, d = gw("NET * PARAM DUMP", 8)
cnt = {}
done = {}
for l in out.splitlines():
    r = re.match(r"NET reply from=(E\d) seq=\d+ part=\d+ PARAM path=", l.strip())
    if r:
        cnt[r.group(1)] = cnt.get(r.group(1), 0) + 1
    r = re.match(r"NET reply from=(E\d) seq=\d+ done parts=(\d+)", l.strip())
    if r:
        done[r.group(1)] = int(r.group(2))
print("   param lines per node:", cnt, " frames:", done)
print("   drops:", d)

print("== NET * SELFTEST (must be refused)")
out, d = gw("NET * SELFTEST", 2)
print("  ", [l.strip() for l in out.splitlines() if "NET" in l][:1])

print("== NET E5 SELFTEST (one node; the others are silent)")
m5 = len(buf["E5"])
out, d = gw("NET E5 SELFTEST", 25)
lines = [l.strip() for l in out.splitlines() if l.startswith("NET reply from=E5")]
print("   reply lines from E5: %d; first: %s" % (len(lines), lines[1][:100] if len(lines) > 1 else lines))
res = [l for l in lines if "result=" in l or "SELFTEST end" in l or "summary" in l.lower()]
for l in res[-4:]:
    print("    ", l[:150])
print("   E5 serial lines during the test: %d" % len(buf["E5"][m5:].splitlines()))
print("   drops:", d)
for s in ports.values():
    s.close()
ser = [l.strip() for l in buf["E5"][m5:].splitlines() if l.strip()]
rep = [re.sub(r"^NET reply from=E5 seq=\d+ part=\d+ ", "", l) for l in lines]
print("   on E5's serial but not in the reply:")
for l in ser:
    if l not in rep and not l.startswith("NET cmd"):
        print("     -", l[:140])
