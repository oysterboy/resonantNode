"""Step 11a board test: E1 is the gateway; the others answer by radio. All stay silent (stored)."""
import re
import sys
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
logs = {n: "" for n in PORTS}


def pump(sec):
    end = time.time() + sec
    while time.time() < end:
        for n, s in ports.items():
            d = s.read(8192)
            if d:
                logs[n] += d.decode(errors="replace")


def mark():
    return {n: len(logs[n]) for n in PORTS}


def since(m, n):
    return logs[n][m[n]:]


def gw(cmd, sec=2.5):
    m = mark()
    ports["E1"].write((cmd + "\n").encode())
    pump(sec)
    return m


def replies(m, seq=None):
    out = {}
    for l in since(m, "E1").splitlines():
        r = re.match(r"NET reply from=(\S+) seq=(\d+) part=(\d+) last=(\d) ?(.*)", l.strip())
        if r and (seq is None or int(r.group(2)) == seq):
            out.setdefault(r.group(1), []).append((int(r.group(3)), int(r.group(4)), r.group(5)))
    return out


def sent_seq(m):
    r = re.search(r"NET sent seq=(\d+)", since(m, "E1"))
    return int(r.group(1)) if r else None


pump(8)  # boot after the flash
print("== boot: radio up per node")
for n in PORTS:
    ports[n].write(b"NET STATUS\n")
pump(1.5)
for n in PORTS:
    st = re.findall(r"NET node=\S+ started=\d[^\n]*", logs[n])
    print("  ", n, st[-1] if st else "(no NET STATUS line)")

print("== NET * NET PING")
m = gw("NET * NET PING")
for k, v in sorted(replies(m, sent_seq(m)).items()):
    print("  ", k, v[-1][2][:110])

print("== NET * PARAM GET output.silent")
m = gw("NET * PARAM GET output.silent")
r = replies(m, sent_seq(m))
for k, v in sorted(r.items()):
    print("  ", k, v[0][2])

print("== NET E3 PARAM SET behavior.rest_ms 6000, then SAVE; reboot E3")
m = gw("NET E3 PARAM SET behavior.rest_ms 6000")
print("   replies:", {k: v[0][2] for k, v in replies(m, sent_seq(m)).items()})
print("   E3 serial:", [l for l in since(m, "E3").splitlines() if l.startswith("NET cmd")][:1])
m = gw("NET E3 PARAM SAVE", 4)
print("   save:", {k: v[0][2] for k, v in replies(m, sent_seq(m)).items()})
m = mark()
ports["E3"].rts = True
time.sleep(0.1)
ports["E3"].rts = False
pump(8)
print("   E3 boot:", [l.strip() for l in since(m, "E3").splitlines() if l.startswith("PARAM load") or l.startswith("RB net") or l.startswith("RB silent")])
m = gw("NET E3 PARAM SET behavior.rest_ms 5000")
m = gw("NET E3 PARAM SAVE", 4)
print("   restored:", {k: v[0][2] for k, v in replies(m, sent_seq(m)).items()})

print("== NET * PARAM DUMP: complete replies?")
m = gw("NET * PARAM DUMP", 6)
r = replies(m, sent_seq(m))
for k, v in sorted(r.items()):
    parts = sorted(p for p, _, _ in v)
    print("   %s: %d parts, numbered 0..%d without gaps: %s, last flag on the final part: %s" % (
        k, len(v), parts[-1], parts == list(range(len(parts))), any(l == 1 and p == parts[-1] for p, l, _ in v)))

print("== not allowed")
m = gw("NET * RB CALL", 1)
print("  ", [l for l in since(m, "E1").splitlines() if "NET" in l][:1])

print("== heap and radio counters (RB STATUS)")
for n in PORTS:
    m = mark()
    ports[n].write(b"RB STATUS\n")
    pump(1.2)
    f = re.findall(r"(net\.\w+|heap\.\w+|i2s\.dropped_dma_buffers|loop\.stack_free_min_bytes)=(\d+)", since(m, n))
    print("  ", n, " ".join("%s=%s" % x for x in f))
for s in ports.values():
    s.close()
