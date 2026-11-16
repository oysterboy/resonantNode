"""Detection rate per call and window: the field run cell by cell through the radio gateway, one log per cell.

  python tools/bench/callmatrix.py --gateway E0=COM12 --players R1,E2 --listeners E3 \
      --calls glide,cricket --windows 250-1250,1250-2250,2500-3500,4000-5000,5500-6500 \
      --secs 600 --out bench/sessions/<folder>

Per cell: the call's preset (tools/bench/presets/<call>.preset) on every board, then one window
(behavior.band_per_node 1, band_range_low_hz / high_hz: one position, nothing moves), players unmuted, listeners
muted, RB log full and node.radio_log 20, all live (nothing saved). After --settle s the cell is logged for --secs from
the boards' radio log ("<t> <name> <line>", cell_<call>_<window>.log). Each window runs every call back to back.

The rate is window-free: with two players every valid detection at one is of the other's call, so player A -> B is
B's valid detections over A's calls; a listener's is its valid detections over all players' calls. summary.csv and
the printed table hold calls, valid / invalid detections and the rates. Ends with every board muted.
"""
import argparse
import csv
import os
import re
import sys
import time

import serial

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nodepreset  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("--gateway", required=True)
ap.add_argument("--players", required=True)
ap.add_argument("--listeners", default="")
ap.add_argument("--calls", default="glide,cricket")
ap.add_argument("--windows", required=True, help="low-high Hz, comma-separated")
ap.add_argument("--secs", type=int, default=600)
ap.add_argument("--settle", type=int, default=10)
ap.add_argument("--out", required=True)
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)
gw = nodepreset.parse(a.gateway)[0]
players = [p for p in a.players.split(",") if p]
listeners = [p for p in a.listeners.split(",") if p]
everyone = players + listeners
LOG = re.compile(r"NET log from=(\S+) (.*)")


def say(text):
    line = "%s %s" % (time.strftime("%H:%M:%S"), text)
    print(line, flush=True)
    with open(os.path.join(a.out, "changes.txt"), "a", encoding="utf-8") as f:
        f.write(line + "\n")


def send(cmds, targets):
    nodepreset.apply_radio(gw, targets, cmds)


def run_cell(call, window):
    lo, hi = window.split("-")
    _, cmds = nodepreset.read_preset(call)
    say("cell %s %s: preset %s, window %s-%s Hz, players %s, listeners %s"
        % (call, window, call, lo, hi, ",".join(players), ",".join(listeners)))
    send(cmds + ["PARAM SET behavior.band_per_node 1", "PARAM SET behavior.band_range_low_hz %s" % lo,
                 "PARAM SET behavior.band_range_high_hz %s" % hi, "PARAM SET node.radio_log 20", "RB log full"],
         everyone)
    send(["PARAM SET output.silent 0"], players)
    if listeners:
        send(["PARAM SET output.silent 1"], listeners)
    time.sleep(a.settle)
    path = os.path.join(a.out, "cell_%s_%s.log" % (call, window))
    s = nodepreset.open_port(gw[1], timeout=0.05)
    t0, buf = time.time(), b""
    with open(path, "w", encoding="utf-8") as f:
        while time.time() - t0 < a.secs:
            chunk = s.read(4096)
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                m = LOG.match(raw.decode("ascii", errors="replace").strip())
                if m:
                    f.write("%.2f %s %s\n" % (time.time() - t0, m.group(1), m.group(2)))
    s.close()
    return count(path)


def count(path):
    calls = {b: 0 for b in everyone}
    valid = {b: 0 for b in everyone}
    invalid = {b: 0 for b in everyone}
    for line in open(path, encoding="utf-8"):
        p = line.split(" ", 2)
        if len(p) < 3 or p[1] not in calls:
            continue
        if p[2].startswith("RB emit shape="):
            calls[p[1]] += 1
        elif p[2].startswith("RB verdict="):
            if "valid=1" in p[2]:
                valid[p[1]] += 1
            else:
                invalid[p[1]] += 1
    return calls, valid, invalid


def pct(n, d):
    return round(100.0 * n / d) if d else None


rows = []
for window in a.windows.split(","):
    for call in a.calls.split(","):
        calls, valid, invalid = run_cell(call, window)
        row = {"call": call, "window": window, "secs": a.secs}
        for b in everyone:
            row["%s_calls" % b] = calls[b]
            row["%s_valid" % b] = valid[b]
            row["%s_invalid" % b] = invalid[b]
        if len(players) == 2:
            x, y = players
            row["%s->%s_pct" % (x, y)] = pct(valid[y], calls[x])
            row["%s->%s_pct" % (y, x)] = pct(valid[x], calls[y])
        total = sum(calls[p] for p in players)
        for b in listeners:
            row["%s_pct" % b] = pct(valid[b], total)
        rows.append(row)
        say("  " + " ".join("%s=%s" % (k, v) for k, v in row.items() if k not in ("call", "window", "secs")))
        with open(os.path.join(a.out, "summary.csv"), "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
send(["PARAM SET output.silent 1"], everyone)
say("done: every board muted (live); the windows and the last call stay live until a reboot")
