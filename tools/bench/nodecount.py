"""Count what a Node build hears over a time window (Node battery for step 5a).

chirps mode: the Emitter on --emit-port is left in AUTO (boots that way) and its
finished chirps are counted; the Node's verdicts are counted from RB log full.
quiet mode: the Emitter is set to REMOTE (silent); every Node verdict is a false
alarm candidate. Verdict lines look like 'RB verdict=... valid=1 ...'.

  python tools/bench/nodecount.py --node COM3 --emit COM4 --mode chirps --secs 100 --out bench/sessions/<folder> --name N_chirps
"""
import argparse, os, re, time
import serial

ap = argparse.ArgumentParser()
ap.add_argument("--node", required=True)
ap.add_argument("--emit", required=True)
ap.add_argument("--mode", choices=["chirps", "quiet"], required=True)
ap.add_argument("--secs", type=int, default=100)
ap.add_argument("--out", required=True)
ap.add_argument("--name", required=True)
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)
n = serial.Serial(a.node, 115200, timeout=0.02)
e = serial.Serial(a.emit, 115200, timeout=0.02)
log = []

def pump(t, stop=None):
    end = time.time() + t
    while time.time() < end:
        for tag, p in (("N", n), ("E", e)):
            l = p.readline()
            if l:
                l = l.decode(errors="replace").rstrip()
                log.append(f"{time.time():.2f} {tag} {l}")
                if stop and tag == "N" and stop in l:
                    return True
    return False

pump(2)
n.write(b"RESET\n") if False else None
# opening the ports reset both boards; wait for the Node baseline (its own quiet gate)
ok = pump(20, "RB baseline active")
if a.mode == "quiet":
    e.write(b"EMIT MODE REMOTE\n"); pump(1)
n.write(b"RB log full\n"); pump(0.5)
start = len(log)
pump(a.secs)
lines = log[start:]
chirps = sum(1 for l in lines if " E " in l and "emitter_chirp_finished" in l)
verd = [l for l in lines if " N " in l and "RB verdict=" in l]
valid = sum(1 for l in verd if "valid=1" in l)
emits = sum(1 for l in lines if " N " in l and "emit start" in l)
open(os.path.join(a.out, a.name + ".log"), "w").write("\n".join(log))
print(f"{a.name}: baseline_ok={ok} secs={a.secs} emitter_chirps={chirps} node_verdicts={len(verd)} valid={valid} node_emit_starts={emits}")
