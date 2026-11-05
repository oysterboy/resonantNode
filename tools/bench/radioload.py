"""What the radio costs the audio: every board's dropped DMA buffers (its own counter) in three phases of one mode,
through a radio gateway board (E0) or a Node on USB:

  quiet        node.radio_log 0, nothing asked
  log          node.radio_log N (default 20: the most the mirror sends), the boards' lines go out
  log + storm  the same, and `NET * PARAM DUMP` every --storm-every seconds (every board answers with ~160 lines in
               8+ replies at once, paced 150 ms apart by the sender)

  python tools/bench/radioload.py COM12 node|echo|whisper [--boards E1,E2,E3,E4,E5] [--secs 60] [--log 20]
                                  [--storm-every 10] [--wav FILE] [--out FILE]

--wav is played in a loop by the lab PC meanwhile (claps for the echo room, a whisper for Whispernet; the Node's boards
make their own traffic). The counters: RB STATUS i2s.dropped_dma_buffers on a Node, ECHO STATUS dropped_bufs= on the
echo and whisper builds. The delivered log lines per second are counted at the gateway. Not saved on the boards.
"""
import argparse
import os
import re
import subprocess
import threading
import time

import serial

ap = argparse.ArgumentParser()
ap.add_argument("port")
ap.add_argument("mode", choices=("node", "echo", "whisper"))
ap.add_argument("--boards", default="E1,E2,E3,E4,E5")
ap.add_argument("--secs", type=float, default=60.0)
ap.add_argument("--log", type=int, default=20)
ap.add_argument("--storm-every", type=float, default=10.0)
ap.add_argument("--wav", default="")
ap.add_argument("--out", default="")
a = ap.parse_args()
boards = a.boards.split(",")
s = serial.Serial()
s.port, s.baudrate, s.timeout = a.port, 115200, 0.05
s.dtr = False
s.rts = False
s.open()
stop = False
text = ""
lock = threading.Lock()


def reader():
    global text
    while not stop:
        d = s.read(65536)
        if d:
            with lock:
                text += d.decode(errors="replace")


threading.Thread(target=reader, daemon=True).start()


def mark():
    with lock:
        return len(text)


def since(i):
    with lock:
        return text[i:]


def send(line, wait=1.5):
    i = mark()
    s.write((line + "\n").encode())
    time.sleep(wait)
    return since(i)


def drops(b):
    cmd, rx = ("RB STATUS", r"i2s\.dropped_dma_buffers=(\d+)") if a.mode == "node" else ("ECHO STATUS", r"dropped_bufs=(\d+)")
    for _ in range(4):
        r = send("NET %s %s" % (b, cmd), 2.8)
        t = " ".join(l for l in r.splitlines() if "from=%s " % b in l)
        m = re.search(rx, t)
        if m:
            return int(m.group(1))
    return None


def player():
    while not stop and a.wav:
        subprocess.run(["powershell", "-NoProfile", "-Command",
                        "(New-Object System.Media.SoundPlayer '%s').PlaySync()" % os.path.abspath(a.wav)], check=False)


out = []


def log(t):
    print(t, flush=True)
    out.append(t)


if a.wav:
    threading.Thread(target=player, daemon=True).start()
    time.sleep(5)
phases = [("quiet", 0, False), ("log %d" % a.log, a.log, False), ("log %d + storm" % a.log, a.log, True)]
log("mode %s, %d boards, %.0f s a phase, wav %s" % (a.mode, len(boards), a.secs, os.path.basename(a.wav) or "-"))
log("%-18s %s   %s" % ("phase", "dropped buffers per board", "log lines/s delivered"))
for name, rate, storm in phases:
    send("NET * PARAM SET node.radio_log %d" % rate, 3.0)
    before = {b: drops(b) for b in boards}
    i0, t0 = mark(), time.time()
    nextStorm = t0
    while time.time() - t0 < a.secs:
        if storm and time.time() >= nextStorm:
            s.write(b"NET * PARAM DUMP\n")
            nextStorm += a.storm_every
        time.sleep(0.2)
    chunk = since(i0)
    dt = time.time() - t0
    after = {b: drops(b) for b in boards}
    d = {b: (after[b] - before[b]) if before[b] is not None and after[b] is not None else None for b in boards}
    lines = len(re.findall(r"NET log from=", chunk))
    log("%-18s %s   %.1f" % (name, "  ".join("%s %s" % (b, "?" if v is None else v) for b, v in d.items()), lines / dt))
send("NET * PARAM SET node.radio_log 20", 3.0)
stop = True
time.sleep(0.3)
s.close()
if a.out:
    open(a.out, "w", encoding="utf-8").write("\n".join(out) + "\n")
