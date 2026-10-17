"""Hearing matrix for a set of Node boards, one pair at a time, the others held silent.

For every pair: the boards not in the pair are opened, silenced (idle off, unreachable detection threshold) and
their ports kept open (closing a port resets a D-AMP board and brings the idle voice back); then
tools/bench/nodepair.py runs the pair (idle-only exchange, RB STATUS polled); then the silent ports are released.
At the end a table of who heard whom.

  python tools/bench/pairmatrix.py --boards E1=COM6,E2=COM3,E3=COM4,E4=COM7 --silent E5=COM9 \
      --secs 180 --out bench/sessions/<folder>
"""
import argparse, itertools, os, re, subprocess, sys, time
import serial

ap = argparse.ArgumentParser()
ap.add_argument("--boards", required=True, help="NAME=PORT,... boards that take part")
ap.add_argument("--silent", default="", help="NAME=PORT,... boards that are only held silent")
ap.add_argument("--secs", type=int, default=180)
ap.add_argument("--pairs", default="", help="optional subset, e.g. E1-E2,E3-E4")
ap.add_argument("--out", required=True)
a = ap.parse_args()

def parse(spec):
    return [tuple(x.split("=")) for x in spec.split(",") if x.strip()]

boards = parse(a.boards)
silent = parse(a.silent)
os.makedirs(a.out, exist_ok=True)
py = sys.executable
here = os.path.dirname(os.path.abspath(__file__))
pairs = list(itertools.combinations(boards, 2))
if a.pairs:
    want = {tuple(p.split("-")) for p in a.pairs.split(",")}
    pairs = [p for p in pairs if (p[0][0], p[1][0]) in want or (p[1][0], p[0][0]) in want]

def hold(ports):
    held = [serial.Serial(p, 115200, timeout=0.05) for p in ports]
    time.sleep(6)
    status = []
    for s in held:
        s.write(b"RB BEHAV idleEnabled=0\n"); time.sleep(0.4)
        s.write(b"PARAM SET detection.scalar_onset_threshold 200000\n"); time.sleep(0.4)
        out = [s.readline().decode("ascii", errors="replace").strip() for _ in range(80)]
        status.append(any("idleEnabled=0" in l for l in out) and any("new=200000" in l for l in out))
    return held, status

results = []
for (na, pa), (nb, pb) in pairs:
    others = [p for n, p in boards + silent if p not in (pa, pb)]
    held, ok = hold(others)
    name = "pair_%s_%s_%ds" % (na, nb, a.secs)
    print("== %s-%s (silent: %s %s)" % (na, nb, ",".join(others), "ok" if all(ok) else "NOT ALL CONFIRMED"), flush=True)
    subprocess.run([py, os.path.join(here, "nodepair.py"), "--a", pa, "--b", pb, "--secs", str(a.secs), "--kick", "none",
                    "--boot-wait", "6", "--status-every", "30", "--out", a.out, "--name", name], capture_output=True, text=True)
    for s in held:
        s.close()
    log = os.path.join(a.out, name + ".log")
    ea = eb = va = vb = 0
    for line in open(log, encoding="utf-8"):
        p = line.split(" ", 2)
        if len(p) < 3:
            continue
        who, rest = p[1], p[2]
        if rest.startswith("BEH emit start"):
            if who == "A": ea += 1
            else: eb += 1
        elif rest.startswith("RB verdict="):
            if who == "A": va += 1
            else: vb += 1
    results.append((na, nb, ea, vb, eb, va))
    print("   %s emits %d -> %s heard %d | %s emits %d -> %s heard %d" % (na, ea, nb, vb, nb, eb, na, va), flush=True)
    time.sleep(2)

print("\nhearing matrix (heard / emitted by the other)")
for na, nb, ea, vb, eb, va in results:
    print("  %s -> %s: %d/%d   %s -> %s: %d/%d" % (na, nb, vb, ea, nb, na, va, eb))
