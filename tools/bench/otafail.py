"""Step 23.1's board gate: a failed OTA during a replay leaves an echo or whisper board answering.

  python tools/bench/otafail.py COM12 E5 echo|whisper WAV [--out FILE]

The board runs the echo or the whisper build. The lab PC plays WAV (claps for the echo room, a whisper for Whispernet);
when the board has decided to answer (ECHO clap / WHISPER heard with consumed_pattern, seen by radio_log) the script
waits until the replay is under way (echo 0.3 s, whisper the board's delay + 1 s) and sends `OTA url=http://<PC>:9/x.bin
token=<the secret>` through the gateway: the URL refuses, the update fails. Then it plays WAV again. Passes when
the board answers ECHO STATUS afterwards, is not stuck in Chirping, and its replays counter rises on the second play.
The token is read from include/wifi_secrets.h and never printed.
"""
import argparse
import re
import subprocess
import threading
import time

import serial

ap = argparse.ArgumentParser()
ap.add_argument("port")
ap.add_argument("board")
ap.add_argument("mode", choices=("echo", "whisper"))
ap.add_argument("wav")
ap.add_argument("--out", default="")
a = ap.parse_args()
tok = re.search(r'#define\s+OTA_TOKEN\s+"([^"]*)"', open("include/wifi_secrets.h", encoding="utf-8").read()).group(1)
B = a.board
s = serial.Serial()
s.port, s.baudrate, s.timeout = a.port, 115200, 0.05
s.dtr = False
s.rts = False
s.open()
buf = ""
lock = threading.Lock()
stop = False


def reader():
    global buf
    while not stop:
        d = s.read(65536)
        if d:
            with lock:
                buf += d.decode(errors="replace")


threading.Thread(target=reader, daemon=True).start()
out = []


def log(t):
    print(t, flush=True)
    out.append(t)


def mark():
    with lock:
        return len(buf)


def since(i):
    with lock:
        return buf[i:]


def send(line, wait=1.5):
    i = mark()
    s.write((line + "\n").encode())
    time.sleep(wait)
    return since(i)


def status():
    for _ in range(4):
        r = send("NET %s ECHO STATUS" % B, 2.8)
        t = " ".join(l for l in r.splitlines() if "from=%s " % B in l)
        m = re.search(r"behavior=(\w+)", t)
        n = re.search(r" replays=(\d+)", t)
        if m and n:
            return m.group(1), int(n.group(1))
    return None


def play_async():
    p = subprocess.Popen(["powershell", "-NoProfile", "-Command",
                          "(New-Object System.Media.SoundPlayer '%s').PlaySync()" % a.wav])
    return p


trigger = (r"NET log from=%s ECHO clap .*decision=consumed_pattern" % B) if a.mode == "echo" else (
    r"NET log from=%s WHISPER heard .*decision=consumed_pattern" % B)
before = status()
log("before: behavior/replays %s" % (before,))
i0 = mark()
p = play_async()
t_end = time.time() + 40
hit = False
while time.time() < t_end and p.poll() is None or time.time() < t_end and not hit:
    if re.search(trigger, since(i0)):
        hit = True
        break
    time.sleep(0.05)
if not hit:
    log("no answer to decide on; the play did not make the board decide to answer: gate not run")
    stop = True
    raise SystemExit(1)
time.sleep(0.3 if a.mode == "echo" else 2.7)
i1 = mark()
s.write(("NET %s OTA url=http://192.168.178.33:9/x.bin token=%s\n" % (B, tok)).encode())
log("failed OTA sent %.1f s after the decision to answer" % 0.0)
time.sleep(45)
chunk = re.sub(r"token=\S+", "token=***", since(i1))
for l in chunk.splitlines():
    if "OTA" in l or "largest_block" in l or "ECHO ota" in l:
        log("  " + l[:150])
mid = status()
log("after the failed OTA: behavior/replays %s" % (mid,))
p.wait()
i2 = mark()
play_async().wait()
time.sleep(8)
after = status()
log("after a second play: behavior/replays %s" % (after,))
ok = bool(mid and after and mid[0] != "Chirping" and after[1] > (mid[1] if mid else 0))
log("RESULT %s: the board %s" % ("PASS" if ok else "FAIL", "answers and replays again" if ok else "did not come back as expected"))
stop = True
time.sleep(0.2)
s.close()
if a.out:
    open(a.out, "w", encoding="utf-8").write("\n".join(out) + "\n")
