"""How complete the radio log is: the boards on a charger send their console lines by radio at most node.radio_log lines
per second (the rest is dropped and counted), so a chain can look shorter than it was.

  python tools/bench/logcomplete.py ping COM4          # NET * NET PING: each board's log=, log_sent=, log_dropped=
  python tools/bench/logcomplete.py eval LOG [--gateway E3] [--from SEC] [--to SEC]

`eval` reads a boardview.py log (--radio boards included). The gateway (a silent USB board) hears every tone of the
window over USB, so its valid verdicts are the truth for "how many tones"; each playing board's logged emits are
compared with it per minute, and each board's logged verdicts with the gateway's. A ratio near 1 means the log is
complete; the log's chains are only trustworthy then (tools/bench/chainlog.py).
"""
import argparse
import re
import sys
import time
from collections import Counter, defaultdict

ap = argparse.ArgumentParser()
ap.add_argument("mode", choices=("ping", "eval"))
ap.add_argument("arg")
ap.add_argument("--gateway", default="E3")
ap.add_argument("--from", dest="t0", type=float, default=0.0)
ap.add_argument("--to", dest="t1", type=float, default=1e12)
a = ap.parse_args()

if a.mode == "ping":
    import serial

    s = serial.Serial()
    s.port, s.baudrate, s.timeout = a.arg, 115200, 0.1
    s.dtr = False
    s.rts = False
    s.open()
    s.reset_input_buffer()
    s.write(b"NET * NET PING\n")
    time.sleep(4)
    text = s.read(65536).decode(errors="replace")
    s.write(b"NET PING\n")
    time.sleep(0.6)
    text += s.read(8192).decode(errors="replace")
    s.close()
    seen = {}
    for m in re.finditer(r"NET node=(E\d) .*?log=(\d+) log_sent=(\d+) log_dropped=(\d+)", text):
        seen[m.group(1)] = tuple(int(g) for g in m.groups()[1:])
    for b in sorted(seen):
        lg, sent, dropped = seen[b]
        share = 100.0 * dropped / (sent + dropped) if sent + dropped else 0.0
        print("%s log=%d lines/s  sent %d  dropped %d (%.0f %% of the lines it tried)" % (b, lg, sent, dropped, share))
    sys.exit(0)

emit, heard, gate = defaultdict(Counter), defaultdict(Counter), Counter()
for line in open(a.arg, encoding="utf-8", errors="replace"):
    p = line.rstrip("\n").split(" ", 2)
    if len(p) < 3:
        continue
    try:
        t = float(p[0])
    except ValueError:
        continue
    if not a.t0 <= t <= a.t1:
        continue
    board, text = p[1], p[2]
    minute = int(t // 60)
    if text.startswith("RB emit accepted"):
        emit[board][minute] += 1
    elif text.startswith("RB verdict="):
        valid = " valid=1" in text
        if board == a.gateway:
            if valid:
                gate[minute] += 1
        elif valid:
            heard[board][minute] += 1

minutes = sorted(set(gate) | {m for b in emit for m in emit[b]})
if not minutes:
    sys.exit("no lines in range")
print("minute  %s heard   emits logged (all players)   ratio" % a.gateway)
tot_g = tot_e = 0
for m in minutes:
    e = sum(emit[b][m] for b in emit if b != a.gateway)
    g = gate[m]
    tot_g += g
    tot_e += e
    print("%6d  %11d   %24d   %s" % (m, g, e, ("%.2f" % (e / g)) if g else "-"))
print("total   %11d   %24d   %.2f" % (tot_g, tot_e, tot_e / tot_g if tot_g else 0.0))
print("\nper board: emits logged / valid verdicts logged (the gateway heard %d valid)" % tot_g)
for b in sorted(set(emit) | set(heard)):
    if b == a.gateway:
        continue
    print("  %s  emits %5d   heard valid %5d" % (b, sum(emit[b].values()), sum(heard[b].values())))
