"""Node bench check (step 11i's bench gate; reusable after any Node restructuring): boot timing, every command's reply,
RB STATUS keys against a reference log, outputs-off and answering, a PARAM SAVE / reboot / load round trip, radio
PING and PARAM SET, SELFTEST, SELFTEST external, SELFTEST by radio. Writes one transcript per board.

  python tools/bench/nodecheck.py --gw E1=COM8 --a E4=COM10 --b E5=COM11 --emitter COM4 --out bench/sessions/<folder>
      [--ref-log bench/sessions/<soak>/soak.log]

Leaves every Node as found: output.silent 1 (stored), the changed param restored and saved, RB log as it was; the
gateway gets PARAM LOAD at the end (idle_enabled and the RB PROFILE reset undone) and its RB log mode back (review
12.0 L21). A PARAM GET that answers no value is a FAIL, not a skipped check (M9).
"""
import argparse
import atexit
import os
import re
import time

import serial

ap = argparse.ArgumentParser()
ap.add_argument("--gw", required=True, help="NAME=PORT of the USB gateway Node")
ap.add_argument("--a", required=True, help="NAME=PORT of a second Node (SELFTEST external listener)")
ap.add_argument("--b", required=True, help="NAME=PORT of a third Node (SELFTEST chirp source)")
ap.add_argument("--emitter", required=True)
ap.add_argument("--out", required=True)
ap.add_argument("--ref-log", help="a soakcycle log of the previous build, for the RB STATUS key set")
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)


def op(p):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = p, 115200, 0.02
    s.dtr = False
    s.rts = False
    s.open()
    return s


names = {}
for role in ("gw", "a", "b"):
    n, p = getattr(a, role).split("=")
    names[role] = n
ports = {names[r]: op(getattr(a, r).split("=")[1]) for r in ("gw", "a", "b")}
ports["EM"] = op(a.emitter)
buf = {n: "" for n in ports}
GW, A, B = names["gw"], names["a"], names["b"]
results = []
# What a run that stops early (Ctrl+C, a serial error) still has to put back (deep review L-C): the param A has
# SAVEd changed, B unsilenced, the gateway's params and log mode.
pending = {"a_param": None, "b_unsilenced": False, "gw_log": None, "done": False}


def put_back():
    if pending["done"]:
        return
    try:
        if pending["a_param"] is not None:
            cmd(A, "PARAM SET behavior.idle_timeout_ms " + pending["a_param"], 0.8)
            cmd(A, "PARAM SAVE", 2.0)
        if pending["b_unsilenced"]:
            cmd(B, "PARAM SET output.silent 1", 0.5)
        cmd(GW, "PARAM LOAD", 1.5)
        if pending["gw_log"]:
            cmd(GW, "RB log " + pending["gw_log"], 0.5)
    except Exception as e:  # the ports may be the reason we stopped
        print("nodecheck: could not put the boards back:", e)
    pending["done"] = True


atexit.register(put_back)


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


def check(name, ok, detail=""):
    results.append((name, ok, detail))
    print("%-34s %s  %s" % (name, "PASS" if ok else "FAIL", detail), flush=True)


def wait_for(n, start, marker, seconds):
    """Pump until `marker` appears in n's output after `start`, or `seconds` pass (a SELFTEST's length varies: it
    waits for quiet and for its own chirps)."""
    end = time.time() + seconds
    while time.time() < end and marker not in buf[n][start:]:
        pump(0.1)
    return marker in buf[n][start:]


def reset(n):
    m = len(buf[n])
    ports[n].rts = True
    time.sleep(0.1)
    ports[n].rts = False
    t0 = time.time()
    seen = {}
    while time.time() - t0 < 30:  # warmup 2 s + the quiet gate's 20 s timeout + margin
        pump(0.05)
        t = buf[n][m:]
        for key in ("BUILD role=", "RB warmup done", "RB baseline active"):
            if key not in seen and key in t:
                seen[key] = round(time.time() - t0, 2)
        if "RB baseline active" in seen:
            break
    return buf[n][m:], seen


pump(1)
# 1. boot
boot, seen = reset(GW)
build = re.findall(r"BUILD role=node git=(\w+)", boot)
check("boot: BUILD, warmup, active", all(k in seen for k in ("BUILD role=", "RB warmup done", "RB baseline active")),
      "build=%s %s" % (build[:1], seen))
check("boot: params loaded", "PARAM load stored=" in boot,
      (re.findall(r"PARAM load stored=\d+ applied=\d+ stale=\d+ invalid=\d+", boot) or ["-"])[0])
gw_log = re.findall(r"RB log mode=(\w+)", cmd(GW, "RB log", 0.8))  # put back at the end (L21)
pending["gw_log"] = gw_log[0] if gw_log else None

# 2. RB STATUS keys vs the reference build
st = cmd(GW, "RB STATUS", 1.5)
keys = set(re.findall(r"(\S+?)=", " ".join(l for l in st.splitlines() if l.startswith("RB STATUS"))))
if a.ref_log:
    ref = ""
    for line in open(a.ref_log, encoding="utf-8", errors="replace"):
        if " %s RB STATUS" % GW in line:
            ref = line
    rkeys = set(re.findall(r"(\S+?)=", ref.split(" RB STATUS", 1)[1])) if ref else set()
    check("RB STATUS key set vs reference", bool(rkeys) and keys == rkeys,
          "only new: %s only ref: %s" % (sorted(keys - rkeys)[:5], sorted(rkeys - keys)[:5]))
check("RB STATUS printed", len(keys) > 30, "%d keys" % len(keys))

# 3. every command in the table answers
for line, expect in (("RB help", "RB CMD"), ("RB summary", "RB"), ("RB log full", "RB log"), ("RB PARAM x", "RB"),
                     ("RB BEHAV x", "RB"), ("PARAM GET output.silent", "path=output.silent"),
                     ("PARAM DUMP", "path=detection.shape_tail_ms"), ("NET STATUS", "NET"),
                     ("XYZ_not_a_command", "RB ERR unknown_command"), ("OTA", "OTA result=bad_command"),
                     ("RB rebase", "RB rebase"), ("RB rebase force", "RB rebase")):
    reply = cmd(GW, line, 3.0 if "rebase" in line else 1.5)
    check("cmd: " + line, expect in reply, (reply.strip().splitlines() or ["(no reply)"])[0][:70])
dump = cmd(GW, "PARAM DUMP", 2.0)
check("PARAM DUMP: 47 params", len(re.findall(r"^PARAM path=", dump, re.M)) == 47,
      "%d" % len(re.findall(r"^PARAM path=", dump, re.M)))
prof = cmd(GW, "RB PROFILE name=TonalPulseScalar", 2.0)
check("cmd: RB PROFILE (reset lines)", "PROFILE" in prof, (prof.strip().splitlines() or ["-"])[0][:70])

# 4. outputs off (silent stored) then answering
for n in (GW,):
    cmd(n, "RB log full", 0.5)
    cmd(n, "PARAM SET behavior.idle_enabled 0", 0.5)
m = len(buf[GW])
for i in range(3):
    ports["EM"].write(b"EMIT PULSES 5 25 10 trial=%d freq=3300 hop=2950,3200,3450,3700,3950\n" % (i + 1))
    pump(6)
t = buf[GW][m:]
check("silent: heard, outputs_off, no answer",
      "decision=outputs_off" in t and "BEH emit start" not in t,
      "verdicts %d outputs_off %d emits %d" % (len(re.findall(r"^RB verdict=", t, re.M)), t.count("outputs_off"),
                                              t.count("BEH emit start")))
cmd(GW, "PARAM SET output.silent 0", 0.5)
m = len(buf[GW])
for i in range(10):
    ports["EM"].write(b"EMIT PULSES 5 25 10 trial=%d freq=3300 hop=2950,3200,3450,3700,3950\n" % (i + 10))
    pump(7)
t = buf[GW][m:]
valid = len(re.findall(r"^RB verdict=.*valid=1", t, re.M))
emits = len(re.findall(r"BEH emit start .*source=heard_pattern", t))
shapes = set(re.findall(r"RB emit shape=hop .*order=([\d,]+)", t))
check("10 hop trains: valid, answered", valid == 10 and emits == 10, "valid %d answers %d order %s" % (valid, emits, shapes))
cmd(GW, "PARAM SET output.silent 1", 0.5)

# 5. PARAM SAVE / reboot / load on A
cur = re.findall(r"value=(\d+)", cmd(A, "PARAM GET behavior.idle_timeout_ms", 1.0))
cur = cur[0] if cur else None
if cur:
    new = str(int(cur) + 1234)
    pending["a_param"] = cur
    cmd(A, "PARAM SET behavior.idle_timeout_ms " + new, 0.8)
    cmd(A, "PARAM SAVE", 2.0)
    bootA, _ = reset(A)
    got = re.findall(r"value=(\d+)", cmd(A, "PARAM GET behavior.idle_timeout_ms", 1.0))
    check("PARAM SAVE / reboot / load", got[:1] == [new], "set %s, after reboot %s" % (new, got[:1]))
    cmd(A, "PARAM SET behavior.idle_timeout_ms " + cur, 0.8)
    cmd(A, "PARAM SAVE", 2.0)
    back = re.findall(r"value=(\d+)", cmd(A, "PARAM GET behavior.idle_timeout_ms", 1.0))
    check("param restored", back[:1] == [cur], "%s" % back[:1])
    if back[:1] == [cur]:
        pending["a_param"] = None
else:
    check("PARAM SAVE / reboot / load", False, "PARAM GET behavior.idle_timeout_ms answered no value")

# 6. radio
r = cmd(GW, "NET * NET PING", 4.0)
froms = sorted(set(re.findall(r"NET reply from=(\w+) seq=\d+ part=0", r)))
check("NET * NET PING", A in froms and B in froms, "replies from %s" % froms)
w = re.findall(r"value=(\d+)", cmd(GW, "PARAM GET behavior.wait_after_heard_ms", 1.0))
if w:
    r = cmd(GW, "NET * PARAM SET behavior.wait_after_heard_ms " + w[0], 4.0)
    oks = sorted(set(re.findall(r"NET reply from=(\w+) .*PARAM SET path=behavior.wait_after_heard_ms.*status=ok", r)))
    check("NET * PARAM SET round trip", A in oks and B in oks, "ok from %s" % oks)
else:
    check("NET * PARAM SET round trip", False, "PARAM GET behavior.wait_after_heard_ms answered no value")

# 7. self-tests
mA = len(buf[A])
ports[A].write(b"SELFTEST\n")
wait_for(A, mA, "SELFTEST_SUMMARY", 90)
r = buf[A][mA:]
checks = re.findall(r"SELFTEST check=(\w+) result=(\w+)", r)
check("SELFTEST on " + A, bool(checks) and all(v in ("PASS", "SKIP") for _, v in checks), str(checks))
pending["b_unsilenced"] = True
cmd(B, "PARAM SET output.silent 0", 0.5)
mA = len(buf[A])
ports[A].write(b"SELFTEST external wait_ms=25000\n")
wait_for(A, mA, "SELFTEST_EXTERNAL waiting", 60)  # the listener must be waiting before the chirps start
mB = len(buf[B])
ports[B].write(b"SELFTEST chirp n=5\n")
wait_for(A, mA, "SELFTEST_SUMMARY", 60)
wait_for(B, mB, "SELFTEST_CHIRP done", 30)
ext = re.findall(r"SELFTEST check=external_chirp result=(\w+)", buf[A][mA:])
check("SELFTEST external (%s hears %s)" % (A, B), ext[:1] == ["PASS"], str(ext))
cmd(B, "PARAM SET output.silent 1", 0.5)
pending["b_unsilenced"] = False
r = cmd(GW, "NET %s SELFTEST" % B, 30.0)
radio = re.findall(r"NET reply from=%s .*SELFTEST check=(\w+) result=(\w+)" % B, r)
check("SELFTEST by radio (NET %s)" % B, len(radio) >= 3, str(radio[:5]))

# the gateway as found (L21): its stored params (idle_enabled, the profile reset), its RB log mode
put_back()

for n in ports:
    open(os.path.join(a.out, "%s.log" % n), "w", encoding="utf-8").write(buf[n])
with open(os.path.join(a.out, "results.txt"), "w", encoding="utf-8") as f:
    for name, ok, detail in results:
        f.write("%-34s %s  %s\n" % (name, "PASS" if ok else "FAIL", detail))
print("%d / %d PASS" % (sum(1 for _, ok, _ in results if ok), len(results)))
