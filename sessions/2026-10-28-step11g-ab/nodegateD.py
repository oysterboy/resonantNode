"""11g batch D bench gate: boot timing with the warmup, 10 hop trains answered, SELFTEST, a bad SELFTEST value."""
import re
import time

import serial


def op(p):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = p, 115200, 0.02
    s.dtr = False
    s.rts = False
    s.open()
    return s


ports = {"E1": op("COM8"), "E3": op("COM4"), "E4": op("COM10")}
buf = {n: "" for n in ports}
stamps = {n: [] for n in ports}


def pump(sec):
    end = time.time() + sec
    while time.time() < end:
        for n, s in ports.items():
            d = s.read(8192)
            if d:
                buf[n] += d.decode(errors="replace")


# 1. boot E1 and time the baseline states
m = len(buf["E1"])
ports["E1"].rts = True
time.sleep(0.1)
ports["E1"].rts = False
t0 = time.time()
seen = {}
while time.time() - t0 < 20:
    pump(0.05)
    t = buf["E1"][m:]
    for key in ("BUILD role=", "RB warmup waiting", "RB warmup done", "RB baseline", "quiet"):
        if key not in seen and key in t:
            seen[key] = round(time.time() - t0, 2)
boot = buf["E1"][m:]
print("boot timing (s after reset):", seen)
for l in boot.splitlines():
    if l.startswith(("BUILD", "RB warmup", "RB baseline")) or "baseline=" in l and "RB" in l:
        print("  ", l.strip()[:150])

# 2. 10 hop trains from E3, E1 answers (silent off, unsaved)
for c in ("PARAM SET output.silent 0", "PARAM SET behavior.idle_enabled 0", "RB log full"):
    ports["E1"].write((c + "\n").encode())
    pump(0.5)
m = len(buf["E1"])
for i in range(10):
    ports["E3"].write(b"EMIT PULSES 5 25 10 trial=%d freq=3300 hop=2950,3200,3450,3700,3950\n" % (i + 1))
    pump(7)
t = buf["E1"][m:]
print("E1, 10 hop trains: valid verdicts %d, answers %d" % (
    len(re.findall(r"^RB verdict=.*valid=1", t, re.M)), len(re.findall(r"BEH emit start .*source=heard_pattern", t))))
ports["E1"].write(b"RB STATUS\n")
pump(1.5)
st = buf["E1"][-3000:]
mm = re.findall(r"(output_busy=\d|behavior\.state=\w+|output\.phase=\w+)", st)
print("RB STATUS sample:", mm[:4])
ports["E1"].write(b"PARAM SET output.silent 1\n")
pump(0.5)
ports["E1"].write(b"RB debug events\n")
pump(1)
print("RB debug reply:", [l.strip() for l in buf["E1"][-400:].splitlines() if "debug" in l.lower()][-2:])

# 3. SELFTEST on E4, then a bad value
m4 = len(buf["E4"])
ports["E4"].write(b"SELFTEST\n")
pump(25)
t4 = buf["E4"][m4:]
print("E4 SELFTEST:", [l.strip()[:120] for l in t4.splitlines() if "check=" in l or "SELFTEST_END" in l][-8:])
m4 = len(buf["E4"])
ports["E4"].write(b"SELFTEST external wait_ms=abc\n")
pump(2)
print("E4 SELFTEST bad value:", [l.strip()[:100] for l in buf["E4"][m4:].splitlines() if l.strip()][:3])
for s in ports.values():
    s.close()
open("C:/Users/admin/AppData/Local/Temp/claude/c----DATA-Nodes-resonantNode/06452682-0eaa-484f-8ae6-05333f655030/scratchpad/nodegateD.out", "w").write(
    "\n\n".join("== %s\n%s" % (n, b) for n, b in buf.items()))
