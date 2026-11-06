"""Who hears whom: each Node calls (RB CALL) n times in turn, the others only listen, and every board's own counters
(heard.valid, heard.strength_sum from RB STATUS, so nothing depends on the lossy radio log) give the matrix of valid
trains heard and their mean strength. Runs through a radio gateway board (E0) or a Node on USB.

  python tools/bench/reach.py COM12 [--boards E1,E2,E3,E4,E5] [--n 10] [--gap 3.5] [--out FILE]

It sets output.silent 1 and behavior.call_enabled 0 on every board live, lets one board play at a time and puts
silent / idle back as the boards had them (silent 1 only on the ones that reported 1; idle 1 everywhere). Needs the
Node image on every board (RB CALL / RB STATUS); not saved, a reboot restores the stored values.
"""
import argparse
import re
import sys
import time

import serial

ap = argparse.ArgumentParser()
ap.add_argument("port")
ap.add_argument("--boards", default="E1,E2,E3,E4,E5")
ap.add_argument("--n", type=int, default=10)
ap.add_argument("--gap", type=float, default=3.5)
ap.add_argument("--out", default="")
a = ap.parse_args()
boards = a.boards.split(",")
s = serial.Serial()
s.port, s.baudrate, s.timeout = a.port, 115200, 0.1
s.dtr = False
s.rts = False
s.open()


def send(line, wait):
    s.reset_input_buffer()
    s.write((line + "\n").encode())
    time.sleep(wait)
    return s.read(65536).decode(errors="replace")


def setv(target, path, value):
    return "status=ok" in send("NET %s PARAM SET %s %s" % (target, path, value), 2.0 if target == "*" else 1.4)


def counters(b):
    for _ in range(4):
        r = send("NET %s RB STATUS" % b, 2.8)
        lines = [l for l in r.splitlines() if "from=%s " % b in l]
        txt = " ".join(lines)
        v = re.search(r"heard\.valid=(\d+)", txt)
        m = re.search(r"heard\.strength_sum=(\d+)", txt)
        sl = re.search(r"silent=(\d)", txt)
        if v and m:
            return int(v.group(1)), int(m.group(1)), sl.group(1) if sl else "?"
    return None


# What each board had, to put it back.
start = {b: counters(b) for b in boards}
was_silent = {b: (start[b][2] == "1") if start[b] else False for b in boards}
out = []
log = lambda t: (print(t, flush=True), out.append(t))
log("start silent: %s" % was_silent)
setv("*", "behavior.call_enabled", 0)
setv("*", "output.silent", 1)
matrix = {}
for caller in boards:
    setv(caller, "output.silent", 0)
    time.sleep(1.0)
    before = {b: counters(b) for b in boards if b != caller}
    for _ in range(a.n):
        send("NET %s RB CALL" % caller, 0.6)
        time.sleep(a.gap)
    time.sleep(1.5)
    after = {b: counters(b) for b in boards if b != caller}
    setv(caller, "output.silent", 1)
    row = {}
    for b in before:
        if before[b] and after[b]:
            d = after[b][0] - before[b][0]
            row[b] = (d, (after[b][1] - before[b][1]) / d if d else 0.0)
        else:
            row[b] = None
    matrix[caller] = row
    log("%s called %d x: %s" % (caller, a.n, "  ".join(
        "%s %s" % (b, ("%d/%d (mean %.0f)" % (v[0], a.n, v[1])) if v else "no reading") for b, v in row.items())))
setv("*", "behavior.call_enabled", 1)
for b in boards:
    setv(b, "output.silent", 1 if was_silent[b] else 0)
log("restored: silent only on %s, idle on" % [b for b in boards if was_silent[b]])
s.close()
if a.out:
    open(a.out, "w", encoding="utf-8").write("\n".join(out) + "\n")
