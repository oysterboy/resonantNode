"""Does the level count a chord by its length? Two boards call back to back (RB CALL, a few tens of ms apart on the
air), a third only listens: its Behavior level before and after, and the verdict lines it logged (duration_ms) for the
pair. Step 23.2's gate on the boards: two answers ~100 ms apart raise the level by 2 (heard_train_ms 125 for the glide).
Through a radio gateway (E0) or a Node on USB; needs node.radio_log lines for the durations.

  python tools/bench/chordcheck.py COM12 [--callers E1,E2] [--listener E5] [--n 12] [--others E3,E4] [--out FILE]

Live, not saved: every board gets start_chance_pct 1 (almost nobody answers), idle off, level decay 600 s (the level only
counts events); the listener is NOT silent (a silent board's Behavior never sees a verdict), the others are. Afterwards start_chance_pct 85 / level_decay 5000 / idle on are put back (the project's
lively settings at 2026-11-05; the script says so, it does not read them from the boards) and the silent flags as they were.
"""
import argparse
import re
import time

import serial

ap = argparse.ArgumentParser()
ap.add_argument("port")
ap.add_argument("--callers", default="E1,E2")
ap.add_argument("--listener", default="E5")
ap.add_argument("--others", default="E3,E4")
ap.add_argument("--n", type=int, default=12)
ap.add_argument("--offset-ms", type=float, default=100.0, help="host-side delay between the two calls (the air adds ~20-30 ms)")
ap.add_argument("--out", default="")
a = ap.parse_args()
callers, listener = a.callers.split(","), a.listener
others = [o for o in a.others.split(",") if o]
allb = callers + [listener] + others
s = serial.Serial()
s.port, s.baudrate, s.timeout = a.port, 115200, 0.05
s.dtr = False
s.rts = False
s.open()


def send(line, wait):
    s.reset_input_buffer()
    s.write((line + "\n").encode())
    end, t = time.time() + wait, ""
    while time.time() < end:
        t += s.read(65536).decode(errors="replace")
    return t


def setv(b, path, v):
    return "status=ok" in send("NET %s PARAM SET %s %s" % (b, path, v), 1.5)


def status(b):
    for _ in range(4):
        r = send("NET %s RB STATUS" % b, 2.8)
        t = " ".join(l for l in r.splitlines() if "from=%s " % b in l)
        lv, sl = re.search(r"behavior\.level=(\d+)", t), re.search(r"silent=(\d)", t)
        if lv:
            return int(lv.group(1)), sl.group(1) if sl else "?"
    return None


was_silent = {b: (status(b) or (0, "0"))[1] == "1" for b in allb}
out = []
log = lambda t: (print(t, flush=True), out.append(t))
for b in allb:
    setv(b, "behavior.start_chance_pct", 1)
    setv(b, "behavior.idle_enabled", 0)
    setv(b, "behavior.level_decay_ms", 600000)
    setv(b, "output.silent", 0 if (b in callers or b == listener) else 1)  # the listener must see verdicts: not silent
send("NET %s RB log full" % listener, 1.5)
time.sleep(2)
log("callers %s listener %s; pair %d times" % (callers, listener, a.n))
rows = []
for k in range(a.n):
    before = status(listener)
    i0 = time.time()
    s.reset_input_buffer()
    s.write(("NET %s RB CALL\n" % callers[0]).encode())
    time.sleep(a.offset_ms / 1000.0)
    s.write(("NET %s RB CALL\n" % callers[1]).encode())
    end, buf = time.time() + 3.0, ""
    while time.time() < end:
        buf += s.read(65536).decode(errors="replace")
    after = status(listener)
    durs = [int(m) for m in re.findall(r"NET log from=%s RB verdict=\S+ .*?valid=1 .*?duration_ms=(\d+)" % listener, buf)]
    d = (after[0] - before[0]) if before and after else None
    rows.append((d, durs))
    log("pair %2d: level +%s, valid verdicts %s" % (k + 1, d, durs))
ok = [r for r in rows if r[0] is not None]
log("level +2 or more in %d of %d pairs; +1 in %d; +0 in %d" % (
    sum(1 for r in ok if r[0] >= 2), len(ok), sum(1 for r in ok if r[0] == 1), sum(1 for r in ok if r[0] <= 0)))
for b in allb:
    setv(b, "behavior.start_chance_pct", 85)
    setv(b, "behavior.idle_enabled", 1)
    setv(b, "behavior.level_decay_ms", 5000)
    setv(b, "output.silent", 1 if was_silent[b] else 0)
log("restored (start chance 85, decay 5000, idle on, silent as before: %s)" % [b for b in allb if was_silent[b]])
s.close()
if a.out:
    open(a.out, "w", encoding="utf-8").write("\n".join(out) + "\n")
