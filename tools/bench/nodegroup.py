"""Several Node builds running freely together (the network as it will be in the trial), others held silent.

All --boards get `RB log full` and run as they are (idle voice, answers, rest rule); --silent boards are
silenced (idle off, unreachable detection threshold) with their ports kept open, since closing a port resets
a D-AMP board. Optional --setup commands go to every board after the boot. RB STATUS is polled every
--status-every s. One log, "<t> <NAME> <line>".

Summary: per board emissions by source, valid verdicts, rests by reason; who-heard-whom, attributing each
valid verdict at a board to the latest emission start of another board 0.15-0.8 s before it (the train is
165 ms, the verdict lands 10-50 ms after its end); turns per minute for the whole group.

  python tools/bench/nodegroup.py --boards E1=COM6,E2=COM3,E3=COM4,E4=COM7 --silent E5=COM9 \
      --secs 600 --status-every 60 --out bench/sessions/<folder> --name group4_600s
"""
import argparse, os, re, time
import serial

ap = argparse.ArgumentParser()
ap.add_argument("--boards", required=True)
ap.add_argument("--silent", default="")
ap.add_argument("--secs", type=int, default=600)
ap.add_argument("--status-every", type=float, default=60.0)
ap.add_argument("--setup", default="", help="';'-separated commands sent to every board after the boot")
ap.add_argument("--out", required=True)
ap.add_argument("--name", required=True)
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)

def parse(spec):
    return [tuple(x.split("=")) for x in spec.split(",") if x.strip()]

boards = parse(a.boards)
silent = parse(a.silent)
ports = {n: serial.Serial(p, 115200, timeout=0.005) for n, p in boards}
held = {n: serial.Serial(p, 115200, timeout=0.05) for n, p in silent}
time.sleep(6)
setup_log = []
for n, s in held.items():
    s.write(b"RB BEHAV idleEnabled=0\n"); time.sleep(0.4)
    s.write(b"PARAM SET detection.scalar_onset_threshold 200000\n"); time.sleep(0.4)
    out = [s.readline().decode("ascii", errors="replace").strip() for _ in range(60)]
    ok = any("idleEnabled=0" in l for l in out) and any("new=200000" in l for l in out)
    setup_log.append("%s silent %s" % (n, "ok" if ok else "NOT CONFIRMED"))
for n, s in ports.items():
    for cmd in [c.strip() for c in a.setup.split(";") if c.strip()]:
        s.write((cmd + "\n").encode()); time.sleep(0.4)
    s.write(b"RB log full\n")
for l in setup_log:
    print(l, flush=True)

events = []
t0 = time.time()
next_status = a.status_every
log_path = os.path.join(a.out, a.name + ".log")
with open(log_path, "w", encoding="utf-8") as f:
    f.write("# nodegroup boards=%s silent=%s secs=%d setup=%s started_unix=%.3f\n" % (a.boards, a.silent, a.secs, a.setup, t0))
    for l in setup_log:
        f.write("# %s\n" % l)
    while time.time() - t0 < a.secs:
        if time.time() - t0 >= next_status:
            for s in ports.values():
                s.write(b"RB STATUS\n")
            next_status += a.status_every
        for n, s in ports.items():
            l = s.readline()
            if not l:
                continue
            l = l.decode("ascii", errors="replace").strip()
            t = time.time() - t0
            f.write("%.2f %s %s\n" % (t, n, l))
            if l.startswith("BEH emit start"):
                m = re.search(r"source=(\w+)", l)
                events.append((t, n, "emit_" + (m.group(1) if m else "?")))
            elif l.startswith("RB verdict=") and "valid=1" in l:
                events.append((t, n, "verdict"))
            elif l.startswith("RB rest start"):
                m = re.search(r"reason=(\w+)", l)
                events.append((t, n, "rest_" + (m.group(1) if m else "?")))
        for s in held.values():
            s.read(4096)
for s in list(ports.values()) + list(held.values()):
    s.close()

names = [n for n, _ in boards]
print("%s: %d s, boards %s" % (a.name, a.secs, ",".join(names)))
for n in names:
    k = lambda kind: sum(1 for _, nn, kk in events if nn == n and kk == kind)
    rests = {kk[5:]: sum(1 for _, nn, x in events if nn == n and x == kk) for kk in {x for _, nn, x in events if nn == n and x.startswith("rest_")}}
    print("  %s: emits heard_pattern=%d idle=%d  verdicts=%d  rests=%s" % (n, k("emit_heard_pattern"), k("emit_idle"), k("verdict"), rests or "{}"))
emits = [(t, n) for t, n, k in events if k.startswith("emit_")]
heard = {(x, y): 0 for x in names for y in names if x != y}
sent = {y: sum(1 for _, n in emits if n == y) for y in names}
unattributed = 0
for t, n, k in events:
    if k != "verdict":
        continue
    cands = [(te, ne) for te, ne in emits if ne != n and 0.15 <= t - te <= 0.8]
    if cands:
        heard[(n, max(cands)[1])] += 1
    else:
        unattributed += 1
print("  who heard whom (listener <- sender: verdicts / sender's emissions):")
for x in names:
    print("    %s <- %s" % (x, "  ".join("%s %d/%d" % (y, heard[(x, y)], sent[y]) for y in names if y != x)))
print("  unattributed verdicts: %d" % unattributed)
mins = int(a.secs // 60) or 1
per_min = [sum(1 for t, _ in emits if m * 60 <= t < (m + 1) * 60) for m in range(mins)]
print("  group emissions per minute: %s" % " ".join(str(x) for x in per_min))
print("  log: %s" % log_path)
