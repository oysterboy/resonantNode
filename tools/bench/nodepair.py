"""Two Node builds listening to each other (step 5b gate: "Node hearing a Node").

Both boards run the Node firmware. The tool turns on RB log full on both, optionally
kicks one of them with a SELFTEST chirp (a plain 3,200 Hz chirp, on a band bin), and
records both serial streams with one clock. At the end it reports per node what was
heard and emitted, and the ping-pong: the sequence of emissions across the two nodes,
the longest alternating chain, and the heard-to-emit latency.

  python tools/bench/nodepair.py --a COM5 --b COM3 --secs 120 --kick a \
      --out bench/sessions/<folder> --name pair_kickA_120s

Lines in the log: "<t> A|B <line>". Verdict lines look like 'RB verdict=... valid=1',
emissions like 'BEH emit start now=... source=heard_pattern|idle|self_test', hop trains
'RB emit shape=hop ... order=...'. Nothing here flashes or resets a board.
"""
import argparse, os, re, time
import serial

ap = argparse.ArgumentParser()
ap.add_argument("--a", required=True, help="COM port of node A")
ap.add_argument("--b", required=True, help="COM port of node B")
ap.add_argument("--secs", type=int, default=120)
ap.add_argument("--kick", choices=["a", "b", "none"], default="a", help="which node plays one SELFTEST chirp at t=5 s")
ap.add_argument("--kick-n", type=int, default=1)
ap.add_argument("--a-setup", default="", help="commands for node A after boot, ';'-separated, e.g. 'PARAM SET output.hop_rotation 3;RB BEHAV idleEnabled=0'")
ap.add_argument("--b-setup", default="", help="same for node B")
ap.add_argument("--boot-wait", type=float, default=4.0, help="seconds to wait after opening the ports (a port open resets a D-AMP board over DTR)")
ap.add_argument("--out", required=True)
ap.add_argument("--name", required=True)
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)

# Opening the port resets the board (DTR), so anything set in an earlier
# process is gone: every setting goes through --a-setup/--b-setup here,
# after the boot, and its reply is printed so a lost command is visible.
ports = {"A": serial.Serial(a.a, 115200, timeout=0.01), "B": serial.Serial(a.b, 115200, timeout=0.01)}
time.sleep(a.boot_wait)
setup_lines = []
for name, cmds in (("A", a.a_setup), ("B", a.b_setup)):
    for cmd in [c.strip() for c in cmds.split(";") if c.strip()]:
        ports[name].write((cmd + "\n").encode())
        time.sleep(0.8)
        reply = []
        while True:
            l = ports[name].readline()
            if not l:
                break
            l = l.decode(errors="replace").strip()
            if l and (l.startswith("PARAM") or l.startswith("RB BEHAV") or l.startswith("RB ERR")):
                reply.append(l[:160])
        setup_lines.append("%s >> %s\n%s" % (name, cmd, "\n".join("%s << %s" % (name, r) for r in reply) or "%s << (no reply captured)" % name))
for line in setup_lines:
    print(line)
for p in ports.values():
    p.write(b"RB log full\n")
t0 = time.time()
kicked = False
events = []  # (t, node, kind, line)
log_path = os.path.join(a.out, a.name + ".log")
with open(log_path, "w", encoding="utf-8") as f:
    f.write("# nodepair a=%s b=%s secs=%d kick=%s kick_n=%d started_unix=%.3f\n" % (a.a, a.b, a.secs, a.kick, a.kick_n, t0))
    for line in setup_lines:
        for sub in line.split("\n"):
            f.write("# setup %s\n" % sub)
    while time.time() - t0 < a.secs:
        t = time.time() - t0
        if not kicked and a.kick != "none" and t >= 5.0:
            ports[a.kick.upper()].write(("SELFTEST chirp n=%d\n" % a.kick_n).encode())
            f.write("%.2f %s >> SELFTEST chirp n=%d\n" % (t, a.kick.upper(), a.kick_n))
            kicked = True
        for name, p in ports.items():
            l = p.readline()
            if not l:
                continue
            l = l.decode(errors="replace").strip()
            t = time.time() - t0
            f.write("%.2f %s %s\n" % (t, name, l))
            kind = None
            if l.startswith("RB verdict="):
                kind = "verdict_valid" if "valid=1" in l else "verdict_invalid"
            elif l.startswith("BEH emit start"):
                m = re.search(r"source=(\w+)", l)
                kind = "emit_" + (m.group(1) if m else "unknown")
            elif l.startswith("RB emit shape=hop"):
                kind = "hop"
            elif "dropped_dma_buffers" in l:
                kind = "status"
            if kind:
                events.append((t, name, kind, l))
for p in ports.values():
    p.close()

# --- summary -------------------------------------------------------------------
def count(node, kind):
    return sum(1 for _, n, k, _ in events if n == node and k == kind)

print("%s: %d s, kick=%s" % (a.name, a.secs, a.kick))
for node in ("A", "B"):
    print("  node %s: verdicts valid=%d invalid=%d  emits heard=%d idle=%d self_test=%d  hop trains=%d" % (
        node, count(node, "verdict_valid"), count(node, "verdict_invalid"), count(node, "emit_heard_pattern"),
        count(node, "emit_idle"), count(node, "emit_self_test"), count(node, "hop")))

# Ping-pong: emissions in time order, alternation chains, heard -> emit latency.
emits = [(t, n, k) for t, n, k, _ in events if k.startswith("emit_")]
chain, longest, prev = 0, 0, None
for t, n, k in emits:
    if k == "emit_heard_pattern" and prev is not None and n != prev:
        chain += 1
    else:
        chain = 1 if k == "emit_heard_pattern" else 0
    longest = max(longest, chain)
    prev = n
lat = []
verd = [(t, n) for t, n, k, _ in events if k == "verdict_valid"]
for t, n, k in emits:
    if k != "emit_heard_pattern":
        continue
    before = [tv for tv, nv in verd if nv == n and 0 <= t - tv < 1.0]
    if before:
        lat.append((t - max(before)) * 1000.0)
print("  emissions in order: %s" % " ".join("%s%s" % (n, {"emit_heard_pattern": "", "emit_idle": "i", "emit_self_test": "t"}.get(k, "?")) for _, n, k in emits[:60]))
print("  longest alternating answer chain: %d  heard->emit latency ms: %s" % (
    longest, ("median %.0f, max %.0f (n=%d)" % (sorted(lat)[len(lat) // 2], max(lat), len(lat))) if lat else "-"))
print("  log: %s" % log_path)
