"""Step 11b bench gate, Node side: E1 answers hop trains, not chirps; SELFTEST external (E4) hears SELFTEST chirp (E5)."""
import re
import time

import serial


def open_port(p):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = p, 115200, 0.02
    s.dtr = False
    s.rts = False
    s.open()
    return s


ports = {"E1": open_port("COM8"), "E3": open_port("COM4"), "E4": open_port("COM10"), "E5": open_port("COM11")}
buf = {n: "" for n in ports}


def pump(sec):
    end = time.time() + sec
    while time.time() < end:
        for n, s in ports.items():
            d = s.read(8192)
            if d:
                buf[n] += d.decode(errors="replace")


def cmd(n, c, sec=1.0):
    m = len(buf[n])
    ports[n].write((c + "\n").encode())
    pump(sec)
    return buf[n][m:]


pump(1)
for c in ("PARAM SET output.silent 0", "PARAM SET behavior.idle_enabled 0", "RB log full"):
    cmd("E1", c, 0.5)
m = len(buf["E1"])
for i in range(10):
    ports["E3"].write(b"EMIT PULSES 5 25 10 trial=%d freq=3300 hop=2950,3200,3450,3700,3950\n" % (i + 1))
    pump(7)
t = buf["E1"][m:]
print("E1, 10 hop trains: valid verdicts %d, answers %d" % (
    len(re.findall(r"^RB verdict=.*valid=1", t, re.M)), len(re.findall(r"BEH emit start .*source=heard_pattern", t))))
m = len(buf["E1"])
for i in range(10):
    ports["E3"].write(b"EMIT CHIRP freq=3450 dur=150 trial=%d\n" % (i + 1))
    pump(4)
t = buf["E1"][m:]
print("E1, 10 chirps (3450 Hz, 150 ms): verdicts %d (valid %d), answers %d" % (
    len(re.findall(r"^RB verdict=", t, re.M)), len(re.findall(r"^RB verdict=.*valid=1", t, re.M)),
    len(re.findall(r"BEH emit start .*source=heard_pattern", t))))
cmd("E1", "PARAM SET output.silent 1", 0.5)

# SELFTEST external on E4, SELFTEST chirp from E5 (E5 must sound: silent off, unsaved)
cmd("E5", "PARAM SET output.silent 0", 0.5)
m4 = len(buf["E4"])
m5 = len(buf["E5"])
ports["E4"].write(b"SELFTEST external wait_ms=25000\n")
pump(3)
ports["E5"].write(b"SELFTEST chirp n=5\n")
pump(40)
t4 = buf["E4"][m4:]
t5 = buf["E5"][m5:]
print("E5 SELFTEST chirp:", [l.strip()[:120] for l in t5.splitlines() if "check=" in l or "RB emit shape" in l][:4])
print("E4 SELFTEST external:", [l.strip()[:160] for l in t4.splitlines() if "check=" in l or "SELFTEST_END" in l or "summary" in l][-4:])
cmd("E5", "PARAM SET output.silent 1", 0.5)
for s in ports.values():
    s.close()
