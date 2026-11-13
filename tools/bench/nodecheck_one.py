"""One-board Node check (step 21.4: every board over the bench, one at a time): the board under test and a radio
gateway on USB, nothing else. Boot timing, the boot's param load, RB STATUS, every command's reply, a PARAM SAVE /
reboot / load round trip, radio PING and PARAM SET from the gateway, SELFTEST over USB, SELFTEST external with a
Node in the room as the chirp source (by radio), SELFTEST by radio. Writes one transcript per port and results.txt.

  python tools/bench/nodecheck_one.py --node R1=COM3 --gw E0=COM12 --source E2 --out bench/sessions/<folder>

--gw: any board that relays NET (the gateway image or a Node). --source: a Node within earshot, reached by radio,
that plays `SELFTEST chirp n=5` for the external check; without it that check is skipped. For the external check
and SELFTEST by radio, every other node is muted by radio (`output.silent 1`, live) and put back to the value each
one reported, so a call from a third node cannot pass the external check.

Not checked here (tools/bench/nodecheck.py, three Nodes and an Emitter on USB): the RB STATUS key set against a
reference build and the heard / answered / outputs_off path with an Emitter's trains. Both are the same firmware
on every board; this script checks what differs per board: mic, amp, radio, flash, the roster name.

Leaves the board as found: the changed param restored and saved, PARAM LOAD (the live changes undone), RB log mode
back, the other nodes' output.silent as reported.
"""
import argparse
import atexit
import os
import re
import time

import serial

ap = argparse.ArgumentParser()
ap.add_argument("--node", required=True, help="NAME=PORT of the board under test (its roster name)")
ap.add_argument("--gw", required=True, help="NAME=PORT of the radio gateway")
ap.add_argument("--source", help="roster name of a Node in earshot, the chirp source for SELFTEST external")
ap.add_argument("--out", required=True)
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)


def op(p):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = p, 115200, 0.02
    s.dtr = False
    s.rts = False
    s.open()
    return s


N, n_port = a.node.split("=")
G, g_port = a.gw.split("=")
ports = {N: op(n_port), G: op(g_port)}
buf = {n: "" for n in ports}
results = []
pending = {"param": None, "log": None, "silent": {}, "done": False}


def pump(sec):
    end = time.time() + sec
    while time.time() < end:
        for n, s in ports.items():
            d = s.read(8192)
            if d:
                buf[n] += d.decode(errors="replace")


def cmd(n, line, sec=1.0):
    m = len(buf[n])
    ports[n].write((line + "\n").encode())
    pump(sec)
    return buf[n][m:]


def wait_for(n, start, marker, seconds):
    end = time.time() + seconds
    while time.time() < end and marker not in buf[n][start:]:
        pump(0.1)
    return marker in buf[n][start:]


def net(line, sec):
    """A NET command from the gateway; waits for every reply's done line, at most `sec` (replies stream at one frame
    per 150 ms)."""
    m = len(buf[G])
    ports[G].write((line + "\n").encode())
    end = time.time() + sec
    while time.time() < end:
        pump(0.2)
        if "done parts" in buf[G][m:] and not line.startswith("NET *"):
            pump(0.5)
            break
    return buf[G][m:]


def check(name, ok, detail=""):
    results.append((name, "PASS" if ok else "FAIL", detail))
    print("%-36s %s  %s" % (name, "PASS" if ok else "FAIL", detail), flush=True)


def skip(name, why):
    results.append((name, "SKIP", why))
    print("%-36s SKIP  %s" % (name, why), flush=True)


def restore_silent():
    for name, old in list(pending["silent"].items()):
        net("NET %s PARAM SET output.silent %s" % (name, old), 4.0)
        del pending["silent"][name]


def put_back():
    if pending["done"]:
        return
    try:
        if pending["param"] is not None:
            cmd(N, "PARAM SET behavior.call_after_silence_ms " + pending["param"], 0.8)
            cmd(N, "PARAM SAVE", 2.0)
        restore_silent()
        cmd(N, "PARAM LOAD", 1.5)
        if pending["log"]:
            cmd(N, "RB log " + pending["log"], 0.5)
    except Exception as e:  # the ports may be the reason we stopped
        print("nodecheck_one: could not put the board back:", e)
    pending["done"] = True


atexit.register(put_back)


def reset():
    # Opening a CP210x port may already have restarted the board (R1, 2026-11-13); the RTS pulse makes it certain.
    m = len(buf[N])
    ports[N].rts = True
    time.sleep(0.1)
    ports[N].rts = False
    t0 = time.time()
    seen = {}
    while time.time() - t0 < 30:  # warmup 2 s + the quiet gate's 20 s timeout + margin
        pump(0.05)
        t = buf[N][m:]
        for key in ("BUILD role=", "RB warmup done", "RB baseline active"):
            if key not in seen and key in t:
                seen[key] = round(time.time() - t0, 2)
        if "RB baseline active" in seen:
            break
    return buf[N][m:], seen


pump(1)
# 1. boot
boot, seen = reset()
build = re.findall(r"BUILD role=node git=(\S+) .*?board=(\S+)", boot)
check("boot: BUILD, warmup, active", all(k in seen for k in ("BUILD role=", "RB warmup done", "RB baseline active")),
      "build=%s %s" % (build[:1], seen))
check("boot: params loaded", "PARAM load stored=" in boot,
      (re.findall(r"PARAM load stored=\d+ applied=\d+ stale=\d+ invalid=\d+", boot) or ["-"])[0])
check("boot: roster name", ("RB node name=%s " % N) in boot,
      (re.findall(r"RB node name=\S+", boot) or ["no name line"])[0])
check("boot: radio started", "RB net started=1" in boot, (re.findall(r"RB net started=\d", boot) or ["-"])[0])
log = re.findall(r"RB log mode=(\w+)", cmd(N, "RB log", 0.8))
pending["log"] = log[0] if log else None

# 2. RB STATUS
st = cmd(N, "RB STATUS", 1.5)
keys = set(re.findall(r"(\S+?)=", " ".join(l for l in st.splitlines() if l.startswith("RB STATUS"))))
check("RB STATUS printed", len(keys) > 30, "%d keys" % len(keys))

# 3. every command answers
for line, expect in (("RB help", "RB CMD"), ("RB summary", "RB"), ("RB log full", "RB log"),
                     ("PARAM GET output.silent", "path=output.silent"), ("PARAM DUMP", "path=detection.shape_tail_ms"),
                     ("NET STATUS", "NET"), ("XYZ_not_a_command", "RB ERR unknown_command"),
                     ("OTA", "OTA result=bad_command"), ("LIGHT off", "LIGHT"), ("RB rebase", "RB rebase")):
    reply = cmd(N, line, 3.0 if "rebase" in line else 1.5)
    hit = [l for l in reply.splitlines() if expect in l]  # the board's own verdict lines interleave with the reply
    check("cmd: " + line, bool(hit), (hit or ["(no reply)"])[0].strip()[:70])

# 4. PARAM SAVE / reboot / load
cur = re.findall(r"value=(\d+)", cmd(N, "PARAM GET behavior.call_after_silence_ms", 1.0))
cur = cur[0] if cur else None
if cur:
    new = str(int(cur) + 1234)
    pending["param"] = cur
    cmd(N, "PARAM SET behavior.call_after_silence_ms " + new, 0.8)
    cmd(N, "PARAM SAVE", 2.0)
    reset()
    got = re.findall(r"value=(\d+)", cmd(N, "PARAM GET behavior.call_after_silence_ms", 1.0))
    check("PARAM SAVE / reboot / load", got[:1] == [new], "set %s, after reboot %s" % (new, got[:1]))
    cmd(N, "PARAM SET behavior.call_after_silence_ms " + cur, 0.8)
    cmd(N, "PARAM SAVE", 2.0)
    back = re.findall(r"value=(\d+)", cmd(N, "PARAM GET behavior.call_after_silence_ms", 1.0))
    check("param restored", back[:1] == [cur], "%s" % back[:1])
    if back[:1] == [cur]:
        pending["param"] = None
else:
    check("PARAM SAVE / reboot / load", False, "PARAM GET behavior.call_after_silence_ms answered no value")
cmd(N, "RB log full", 0.5)

# 5. radio
time.sleep(2)  # the radio after the reboot
r = net("NET %s NET PING" % N, 4.0)
check("NET PING from %s" % G, ("NET reply from=%s " % N) in r,
      (re.findall(r"NET reply from=%s seq=\d+ part=0 (.*)" % N, r) or ["no reply"])[0][:70])
w = re.findall(r"value=(\d+)", cmd(N, "PARAM GET behavior.wait_after_heard_ms", 1.0))
if w:
    r = net("NET %s PARAM SET behavior.wait_after_heard_ms %s" % (N, w[0]), 4.0)
    check("NET PARAM SET round trip", bool(re.search(r"NET reply from=%s .*PARAM SET .*status=ok" % N, r)),
          "value %s" % w[0])
else:
    check("NET PARAM SET round trip", False, "PARAM GET behavior.wait_after_heard_ms answered no value")

# 6. the others muted by radio before every self-test, each one's old value kept for the put-back. A neighbour's
# call inside the own-chirp window fails own_chirp_suppressed (R1, 2026-11-13). Every node that answers a ping is
# muted by name until it confirms: a node that restarted in between is unmuted and its call would pass the
# external check (E3, 2026-11-13).
others = sorted(set(re.findall(r"NET node=(\w+) ", net("NET * NET PING", 5.0))) - {N})
for name in others:
    for attempt in range(3):
        r = net("NET %s PARAM SET output.silent 1" % name, 4.0)
        got = re.findall(r"NET reply from=%s .*?PARAM SET path=output.silent .*?old=(\d)" % name, r)
        if got:
            pending["silent"].setdefault(name, got[0])
            break
muted = sorted(pending["silent"])
check("others muted by radio", muted == others, "pinged %s, muted %s" % (others, muted))
cmd(N, "PARAM SET output.silent 0", 0.5)
# 7. SELFTEST over USB (the call off, so the node's own call cannot land in the amp check)
cmd(N, "PARAM SET behavior.call_enabled 0", 0.5)
cmd(N, "PARAM SET output.silent 0", 0.5)
m = len(buf[N])
ports[N].write(b"SELFTEST\n")
wait_for(N, m, "SELFTEST_SUMMARY", 90)
r = buf[N][m:]
checks = re.findall(r"SELFTEST check=(\w+) result=(\w+)", r)
amp = re.findall(r"check=amp result=\w+.*?snr_min_db=([\d.]+) snr_max_db=([\d.]+) tone_spread=([\d.]+)", r)
mic = re.findall(r"check=mic result=\w+.*?bit8_pct=([\d.]+) framing=(\w+)", r)
check("SELFTEST on " + N, bool(checks) and all(v in ("PASS", "SKIP") for _, v in checks),
      "%s mic(bit8,framing)=%s amp(snr min,max,spread)=%s" % (checks, mic[:1], amp[:1]))

if a.source:
    if a.source not in muted:
        check("SELFTEST external (%s hears %s)" % (N, a.source), False, "%s did not answer by radio" % a.source)
    else:
        net("NET %s PARAM SET output.silent 0" % a.source, 4.0)
        m = len(buf[N])
        ports[N].write(b"SELFTEST external wait_ms=25000\n")
        wait_for(N, m, "SELFTEST_EXTERNAL waiting", 60)  # the listener must be waiting before the chirps start
        net("NET %s SELFTEST chirp n=5" % a.source, 3.0)
        wait_for(N, m, "SELFTEST_SUMMARY", 60)
        ext = re.findall(r"SELFTEST check=external_chirp result=(\w+)", buf[N][m:])
        check("SELFTEST external (%s hears %s)" % (N, a.source), ext[:1] == ["PASS"], str(ext))
        net("NET %s PARAM SET output.silent 1" % a.source, 4.0)
else:
    skip("SELFTEST external", "no --source")
time.sleep(3)
r = net("NET %s SELFTEST" % N, 40.0)
radio = re.findall(r"NET reply from=%s .*SELFTEST check=(\w+) result=(\w+)" % N, r)
check("SELFTEST by radio (NET %s)" % N, len(radio) >= 3 and all(v in ("PASS", "SKIP") for _, v in radio),
      str(radio[:6]))
restore_silent()
check("others put back", not pending["silent"], "muted and restored: %s" % muted)

put_back()

for n in ports:
    open(os.path.join(a.out, "%s.log" % n), "w", encoding="utf-8").write(buf[n])
with open(os.path.join(a.out, "results.txt"), "w", encoding="utf-8") as f:
    for name, res, detail in results:
        f.write("%-36s %s  %s\n" % (name, res, detail))
print("%d / %d PASS, %d SKIP" % (sum(1 for _, r_, _ in results if r_ == "PASS"),
                                sum(1 for _, r_, _ in results if r_ != "SKIP"),
                                sum(1 for _, r_, _ in results if r_ == "SKIP")))
