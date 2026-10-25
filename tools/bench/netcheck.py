"""Radio remote-command check (step 11a, PAR-013): function and audio cost of NET on a set of Nodes, one command.

One board is the gateway (it sends `NET <target> <command>`); the others answer by radio. Every step reads
i2s.dropped_dma_buffers on every board before and after (RB STATUS over USB), so each line says what the step
cost the audio. Run it on silent nodes (`output.silent 1` stored) unless a step's sound is wanted.

Steps: radio idle; NET PING; PARAM GET; PARAM SET to one node + SAVE + reboot (kept?), then restored; RB STATUS
(one ~1.6 KB line per node, joined at the gateway); PARAM DUMP (paced); NET * SELFTEST (must be refused); with
--selftest NAME, a SELFTEST on that one node (plays tones; the others must be silent: operator rule).

  python tools/bench/netcheck.py --gateway E1=COM8 --nodes E2=COM3,E3=COM4,E4=COM10,E5=COM11 \
      --persist-node E3 [--selftest E5] [--idle-s 120] [--out bench/sessions/<folder>]
"""
import argparse
import os
import re
import sys
import time

import serial

ap = argparse.ArgumentParser()
ap.add_argument("--gateway", required=True, help="NAME=PORT")
ap.add_argument("--nodes", required=True, help="NAME=PORT,... (the ones answering by radio)")
ap.add_argument("--persist-node", help="NAME: SET + SAVE + reboot check on this node (behavior.rest_ms, restored)")
ap.add_argument("--selftest", help="NAME: run SELFTEST by radio on this one node (plays tones)")
ap.add_argument("--idle-s", type=float, default=60.0)
ap.add_argument("--out", help="folder: netcheck.log (everything read) and netcheck.txt (this report)")
a = ap.parse_args()

gw_name, gw_port = a.gateway.split("=")
boards = [(gw_name, gw_port)] + [tuple(x.split("=")) for x in a.nodes.split(",") if x]
names = [n for n, _ in boards]
ports = {}
for n, p in boards:
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = p, 115200, 0.02
    s.dtr = False
    s.rts = False
    s.open()
    ports[n] = s
buf = {n: "" for n in names}
report = []
failures = []


def say(line):
    print(line, flush=True)
    report.append(line)


def pump(sec):
    end = time.time() + sec
    while time.time() < end:
        for n, s in ports.items():
            d = s.read(8192)
            if d:
                buf[n] += d.decode(errors="replace")


def mark():
    return {n: len(buf[n]) for n in names}


def drops():
    m = mark()
    for s in ports.values():
        s.write(b"RB STATUS\n")
    pump(1.5)
    return {n: int((re.findall(r"i2s\.dropped_dma_buffers=(\d+)", buf[n][m[n]:]) or ["-1"])[-1]) for n in names}


def gateway(cmd, wait):
    m = mark()
    ports[gw_name].write((cmd + "\n").encode())
    pump(wait)
    return m, buf[gw_name][m[gw_name]:]


def replies(text):
    out = {}
    for l in text.splitlines():
        r = re.match(r"NET reply from=(\S+) seq=\d+ part=\d+ (.*)", l.strip())
        if r:
            out.setdefault(r.group(1), []).append(r.group(2))
    return out


def step(title, action, check):
    before = drops()
    result = action()
    after = drops()
    cost = {n: after[n] - before[n] for n in names}
    ok, detail = check(result)
    say("%-4s %-38s %s | drops %s" % ("ok" if ok else "FAIL", title, detail,
                                     " ".join("%s:+%d" % (n, cost[n]) for n in names)))
    if not ok:
        failures.append(title)
    return result


others = [n for n in names if n != gw_name]
pump(2)
step("radio idle %.0f s" % a.idle_s, lambda: pump(a.idle_s), lambda r: (True, "-"))
step("NET * NET PING", lambda: gateway("NET * NET PING", 3)[1],
     lambda t: (set(replies(t)) >= set(others), "answered: %s" % ",".join(sorted(replies(t)))))
step("NET * PARAM GET output.silent", lambda: gateway("NET * PARAM GET output.silent", 3)[1],
     lambda t: (set(replies(t)) >= set(others), " ".join("%s=%s" % (k, re.findall(r"value=(\S+)", " ".join(v))[:1]) for k, v in sorted(replies(t).items()))))

if a.persist_node:
    pn = a.persist_node

    def persist():
        gateway("NET %s PARAM SET behavior.rest_ms 6000" % pn, 2.5)
        gateway("NET %s PARAM SAVE" % pn, 4)
        m = mark()
        ports[pn].rts = True
        time.sleep(0.1)
        ports[pn].rts = False
        pump(9)
        boot = buf[pn][m[pn]:]
        gateway("NET %s PARAM SET behavior.rest_ms 5000" % pn, 2.5)
        gateway("NET %s PARAM SAVE" % pn, 4)
        return boot

    step("SET + SAVE on %s, reboot" % pn, persist,
         lambda boot: ("PARAM load path=behavior.rest_ms value=6000" in boot, "boots with rest_ms 6000: %s" % ("PARAM load path=behavior.rest_ms value=6000" in boot)))

step("NET * RB STATUS", lambda: gateway("NET * RB STATUS", 6)[1],
     lambda t: (all(any(x.startswith("RB STATUS") and "board=" in x[-40:] for x in replies(t).get(n, [])) for n in others),
                " ".join("%s:%d" % (k, max((len(x) for x in v if x.startswith("RB STATUS")), default=0)) for k, v in sorted(replies(t).items()))))
step("NET * PARAM DUMP", lambda: gateway("NET * PARAM DUMP", 8)[1],
     lambda t: (all(sum(1 for x in replies(t).get(n, []) if x.startswith("PARAM path=")) >= 30 for n in others),
                " ".join("%s:%d" % (k, sum(1 for x in v if x.startswith("PARAM path="))) for k, v in sorted(replies(t).items()))))
step("NET * SELFTEST (refused)", lambda: gateway("NET * SELFTEST", 2)[1],
     lambda t: ("selftest_needs_one_target" in t, "refused: %s" % ("selftest_needs_one_target" in t)))
if a.selftest:
    st = a.selftest
    step("NET %s SELFTEST" % st, lambda: gateway("NET %s SELFTEST" % st, 30)[1],
         lambda t: ("SELFTEST_END" in " ".join(replies(t).get(st, [])) or any("result=" in x for x in replies(t).get(st, [])),
                    "results: %s" % " ".join(re.findall(r"check=(\w+) result=(\w+)", " ".join(replies(t).get(st, [])))[i][0] + "=" +
                                             re.findall(r"check=(\w+) result=(\w+)", " ".join(replies(t).get(st, [])))[i][1]
                                             for i in range(len(re.findall(r"check=(\w+) result=(\w+)", " ".join(replies(t).get(st, []))))))))

for s in ports.values():
    s.close()
say("result: %s" % ("PASS" if not failures else "FAIL: " + ", ".join(failures)))
if a.out:
    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, "netcheck.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report) + "\n")
    with open(os.path.join(a.out, "netcheck.log"), "w", encoding="utf-8") as f:
        for n in names:
            f.write("##### %s\n%s\n" % (n, buf[n]))
sys.exit(1 if failures else 0)
