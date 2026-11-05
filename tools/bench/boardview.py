"""What runs on the boards, live: one row per board (its mode, state, band or delay, level, last action, counters),
in the terminal and as a page that refreshes itself in the browser.

  python tools/bench/boardview.py --boards E1=COM8,E2=COM3,E3=COM4,E4=COM10,E5=COM11 [--every 5] [--html out.html]
      [--log out.log] [--radio E1,E4,E5]

It opens the ports without resetting the boards (DTR / RTS low), asks each board every --every seconds for its
status (RB STATUS on a Node, ECHO STATUS on the echo shell, which adds WHISPER status in the whisper build; a
board ignores the other's command), and follows what the boards print anyway (emits, verdicts, replays). --log keeps
every line. Ctrl+C ends it. It holds the ports: stop it before a run that needs them (nodegroup, OTA scripts).
"""
import argparse
import html
import os
import re
import threading
import time

import serial

ap = argparse.ArgumentParser()
ap.add_argument("--boards", required=True)
ap.add_argument("--every", type=float, default=5.0)
ap.add_argument("--html", default="")
ap.add_argument("--log", default="")
ap.add_argument("--radio", default="", help="boards without USB, e.g. E1,E4,E5: their node.radio_log lines and "
                "NET replies, as printed by the first --boards board (a Node there also asks NET * RB STATUS)")
a = ap.parse_args()

KV = re.compile(r"(\S+?)=(\S*)")
boards = {}
for spec in a.boards.split(","):
    name, port = spec.split("=")
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.05
    s.dtr = False
    s.rts = False
    s.open()
    boards[name] = {"s": s, "port": port, "mode": "?", "state": "?", "where": "", "level": "", "last": "",
                    "last_t": 0.0, "emits": 0, "answers": 0, "heard": 0, "extra": "", "seen": 0.0}
gateway = next(iter(boards))
for name in filter(None, a.radio.split(",")):
    boards[name] = {"s": None, "port": "radio", "mode": "?", "state": "?", "where": "", "level": "", "last": "",
                    "last_t": 0.0, "emits": 0, "answers": 0, "heard": 0, "extra": "", "seen": 0.0}
RADIO = re.compile(r"NET (?:log from=(\S+) |reply from=(\S+) seq=\d+ part=\d+ )(.*)")
logf = open(a.log, "a", encoding="utf-8") if a.log else None
lock = threading.Lock()
t0 = time.time()


def note(b, text):
    b["last"] = text
    b["last_t"] = time.time()


def on_line(name, line):
    m = RADIO.match(line)
    if m:
        # Another board's line, by radio: only through the gateway (every board on USB prints the same frames).
        who = m.group(1) or m.group(2)
        if name == gateway and who in boards and boards[who]["s"] is None:
            on_line(who, m.group(3))
        return
    b = boards[name]
    b["seen"] = time.time()
    if logf:
        logf.write("%.2f %s %s\n" % (time.time() - t0, name, line))
        logf.flush()
    if line.startswith("RB STATUS"):
        kv = dict(KV.findall(line))
        b["mode"] = "node"
        b["state"] = kv.get("behavior_state", "?") + (" rest" if kv.get("behavior.resting") == "1" else "")
        b["level"] = kv.get("behavior.level", "")
        base = kv.get("behavior.base_bin")
        b["where"] = "base %s" % base if base is not None else ""
        b["extra"] = "heard %s" % kv.get("heard.valid", "")
    elif line.startswith("ECHO status"):
        kv = dict(KV.findall(line))
        if b["mode"] != "whisper":
            b["mode"] = "echo"
        b["state"] = kv.get("behavior", "?")
        b["level"] = kv.get("level_auto", "")
        if b["mode"] == "echo":
            b["extra"] = "claps %s replays %s" % (kv.get("claps", ""), kv.get("replays", ""))
    elif line.startswith("WHISPER status"):
        kv = dict(KV.findall(line))
        b["mode"] = "whisper"
        b["where"] = "D %s ms, room %s, onset %s" % (kv.get("delay_ms", ""), kv.get("room", "-"), kv.get("onset_now", "-"))
        b["extra"] = "phrases %s answered %s" % (kv.get("phrases", ""), kv.get("answered", ""))
    elif line.startswith("RB emit accepted"):
        src = re.search(r"source=(\w+)", line)
        b["emits"] += 1
        if src and src.group(1) == "heard_pattern":
            b["answers"] += 1
        note(b, "emit " + (src.group(1) if src else ""))
    elif line.startswith("RB emit shape=hop"):
        m = re.search(r"order=([\d,]+)", line)
        if m:
            hz = m.group(1).split(",")
            b["where"] = "%s-%s Hz" % (hz[0], hz[-1]) + ("  " + b["where"] if b["where"].startswith("base") else "")
    elif line.startswith("RB verdict=") and "valid=1" in line:
        b["heard"] += 1
        m = re.search(r"decision=(\w+)", line)
        note(b, "heard -> " + (m.group(1) if m else ""))
    elif line.startswith("ECHO clap"):
        b["heard"] += 1
        m = re.search(r"decision=(\w+)", line)
        note(b, "clap -> " + (m.group(1) if m else ""))
    elif line.startswith("ECHO replay"):
        b["emits"] += 1
        note(b, "replay")
    elif line.startswith("WHISPER heard"):
        b["heard"] += 1
        m = re.search(r"decision=(\w+)", line)
        note(b, "whisper -> " + (m.group(1) if m else ""))


def reader(name):
    b = boards[name]
    buf = ""
    while True:
        try:
            d = b["s"].read(4096)
        except serial.SerialException:
            time.sleep(1)
            continue
        if not d:
            continue
        buf += d.decode(errors="replace")
        while "\n" in buf:
            line, buf = buf.split("\n", 1)
            with lock:
                on_line(name, line.strip())


def rows():
    now = time.time()
    out = []
    for name, b in boards.items():
        age = "%.0fs ago" % (now - b["last_t"]) if b["last_t"] else ""
        alive = "" if now - b["seen"] < 3 * a.every + 5 else ("  (silent port)" if b["s"] else "  (no radio lines)")
        out.append((name, b["mode"], b["state"], b["where"], b["level"], "%s %s" % (b["last"], age),
                    "emits %d answers %d heard %d" % (b["emits"], b["answers"], b["heard"]), b["extra"] + alive))
    return out


HEAD = ("board", "mode", "state", "band / delay", "level", "last action", "since start", "counters")


def render():
    r = rows()
    widths = [max(len(str(x[i])) for x in r + [HEAD]) for i in range(len(HEAD))]
    line = lambda x: "  ".join(str(v).ljust(w) for v, w in zip(x, widths))
    text = "\033[2J\033[H" + time.strftime("%H:%M:%S") + "  (Ctrl+C ends)\n" + line(HEAD) + "\n" + "\n".join(line(x) for x in r)
    print(text, flush=True)
    if a.html:
        cells = "".join("<tr>" + "".join("<td>%s</td>" % html.escape(str(v)) for v in x) + "</tr>" for x in r)
        page = ("<!doctype html><meta charset=utf-8><meta http-equiv=refresh content=%d><title>Boards</title>"
                "<style>body{font:14px system-ui;margin:16px}table{border-collapse:collapse}td,th{padding:4px 10px;"
                "border-bottom:1px solid #ccc;text-align:left}</style><h3>Boards %s</h3><table><tr>%s</tr>%s</table>"
                % (max(2, int(a.every)), time.strftime("%H:%M:%S"),
                   "".join("<th>%s</th>" % h for h in HEAD), cells))
        tmp = a.html + ".tmp"
        open(tmp, "w", encoding="utf-8").write(page)
        os.replace(tmp, a.html)


for n in boards:
    threading.Thread(target=reader, args=(n,), daemon=True).start()
try:
    while True:
        for b in boards.values():
            if b["s"] is None:
                continue
            try:
                b["s"].write(b"RB STATUS\nECHO STATUS\n")
            except serial.SerialException:
                pass
        if a.radio and boards[gateway]["mode"] == "node":
            boards[gateway]["s"].write(b"NET * RB STATUS\n")  # an echo board cannot send; log lines still come
        time.sleep(1.0)
        with lock:
            render()
        time.sleep(max(0.0, a.every - 1.0))
except KeyboardInterrupt:
    pass
