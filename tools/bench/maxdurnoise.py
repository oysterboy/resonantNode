"""Max duration A/B under music and speech (step 14 follow-up, owner 2026-10-29: revisit
detection.scalar_max_duration_ms so chords 120-220 ms apart are not lost). One Node keeps the shipped limit, the
others run the candidate (live PARAM SET, not saved); all listen silent. First --trains hop10 trains from the
Emitter (valid trains must still pass), then bench_audio.wav played --plays times from this PC (noise must stay out).

  python tools/bench/maxdurnoise.py --ref E1=COM8 --test E4=COM10 --test E5=COM11 --emitter COM4 \
      --candidate 450 --out bench/sessions/<folder> [--plays 2] [--trains 10]

Leaves the Nodes as found (PARAM LOAD, RB log mode back), also on Ctrl+C or an error. PARAM LOAD restores what is
stored: a value never PARAM SAVEd stays live until a reboot. The --ref Node runs whatever max duration it has
(stored, else the firmware default: 450 since 2026-10-29), so pass the value under test that differs from it.
"""
import argparse
import atexit
import os
import re
import subprocess
import time

import serial

AUDIO = r"C:\Users\admin\Downloads\bench_audio.wav"
ap = argparse.ArgumentParser()
ap.add_argument("--ref", required=True)
ap.add_argument("--test", action="append", required=True)
ap.add_argument("--emitter", required=True)
ap.add_argument("--candidate", type=int, required=True)
ap.add_argument("--out", required=True)
ap.add_argument("--plays", type=int, default=2)
ap.add_argument("--trains", type=int, default=10)
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)


def op(port):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.02
    s.dtr = False
    s.rts = False
    s.open()
    return s


specs = [a.ref] + a.test
nodes = {x.split("=")[0]: op(x.split("=")[1]) for x in specs}
REF = a.ref.split("=")[0]
em = op(a.emitter)
log = {n: [] for n in nodes}
part = {n: "" for n in nodes}


def pump(sec):
    end = time.time() + sec
    while time.time() < end:
        for n, s in nodes.items():
            d = s.read(8192)
            if d:
                part[n] += d.decode(errors="replace")
                *lines, part[n] = part[n].split("\n")
                now = time.time()
                log[n].extend((now, l.rstrip("\r")) for l in lines)
        em.read(8192)


def cmd(n, line, sec=0.5):
    nodes[n].write((line + "\n").encode())
    pump(sec)


def count(n, t0, t1):
    seg = [l for t, l in log[n] if t0 <= t < t1]
    return dict(verdicts=sum(1 for l in seg if l.startswith("RB verdict=")),
                valid=sum(1 for l in seg if l.startswith("RB verdict=") and "valid=1" in l),
                too_long=sum(1 for l in seg if "duration_too_long" in l))


pump(0.5)
modes = {}
restored = []


def restore():
    if restored:
        return
    restored.append(True)
    for n in nodes:
        cmd(n, "PARAM LOAD", 1.2)
        cmd(n, "RB log " + modes.get(n, "minimal"))


atexit.register(restore)
for n in nodes:
    m0 = len(log[n])
    cmd(n, "RB log", 0.6)
    found = [re.search(r"RB log mode=(\w+)", l) for _, l in log[n][m0:]]
    modes[n] = next((m.group(1) for m in found if m), "minimal")
    cmd(n, "RB log full")
    cmd(n, "PARAM SET behavior.idle_enabled 0")
    cmd(n, "PARAM SET output.silent 1")
    if n != REF:
        cmd(n, "PARAM SET detection.scalar_max_duration_ms %d" % a.candidate)
pump(2)

t0 = time.time()
for i in range(a.trains):
    em.write(b"EMIT PULSES 5 25 10 trial=%d freq=3450 hop=2950,3200,3450,3700,3950\n" % (i + 1))
    pump(4)
t1 = time.time()
print("hop10 x%d:" % a.trains, {n: count(n, t0, t1) for n in nodes}, flush=True)

player = subprocess.Popen(["powershell", "-NoProfile", "-Command",
                           "$p = New-Object System.Media.SoundPlayer '%s'; for ($i = 0; $i -lt %d; $i++) { $p.PlaySync() }"
                           % (AUDIO, a.plays)])
t2 = time.time()
while player.poll() is None:
    pump(1)
pump(3)
t3 = time.time()
print("noise %d plays (%.0f s):" % (a.plays, t3 - t2), {n: count(n, t2, t3) for n in nodes}, flush=True)

restore()
for n in nodes:
    with open(os.path.join(a.out, "%s.log" % n), "w", encoding="utf-8") as f:
        for t, l in log[n]:
            f.write("%.3f %s\n" % (t, l))
with open(os.path.join(a.out, "results.txt"), "w", encoding="utf-8") as f:
    f.write("ref %s at the stored limit, test %s at %d ms\n" % (REF, [x.split("=")[0] for x in a.test], a.candidate))
    f.write("hop10 x%d: %s\n" % (a.trains, {n: count(n, t0, t1) for n in nodes}))
    f.write("noise %d plays: %s\n" % (a.plays, {n: count(n, t2, t3) for n in nodes}))
