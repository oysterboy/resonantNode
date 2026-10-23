"""Several Node builds running freely together (the network as it will be in the trial), others held silent.

All --boards get `RB log full` and run as they are (idle voice, answers, rest rule); --silent boards are
silenced (idle off, unreachable detection threshold) with their ports kept open, since closing a port resets
a D-AMP board. Optional --setup commands go to every board after the boot, then every free-running board
is asked for `PARAM DUMP` (behavior.*, detection.*, output.*) and the replies go into the log header as
`# state <NAME> << ...` lines (the effective values the run used, not only the commands sent;
decisions/2026-10-17-behavior-contract-and-reproducible-sessions.md). RB STATUS is polled every
--status-every s. One log, "<t> <NAME> <line>".

Summary: per board emissions by source, valid verdicts, rests by reason; who-heard-whom, attributing each
valid verdict at a board to the latest emission start of another board 0.15-0.8 s before it (the train is
165 ms, the verdict lands 10-50 ms after its end); turns per minute for the whole group. Strength drift (step
10c): median heard strength per listener <- sender per --drift-bin s (from the verdict lines' strength=, Node
da771a5 or later), and each board's mean valid strength per bin from RB STATUS heard.valid / heard.strength_sum.

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
ap.add_argument("--kick", default="", help="NAME: send RB CALL to this board (step 5g kick) at --kick-first s, then every --kick-every s")
ap.add_argument("--kick-first", type=float, default=15.0)
ap.add_argument("--kick-every", type=float, default=90.0)
ap.add_argument("--quiet-gap", type=float, default=15.0, help="a silence this long counts as 'quiet' for time-to-quiet")
ap.add_argument("--quiet-within", type=float, default=30.0, help="pass mark: quiet within this many s after a start (RB CALL or idle call)")
ap.add_argument("--chain-gap", type=float, default=3.0, help="a silence this long ends a chain (turns are ~0.35 s apart)")
ap.add_argument("--max-per-min", type=int, default=100, help="M: the group's emissions-per-minute ceiling (owner 2026-10-22)")
ap.add_argument("--drift-bin", type=float, default=3600.0, help="bin width (s) of the strength drift tables; 0 = off")
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
    s.write(b"PARAM SET behavior.idle_enabled 0\n"); time.sleep(0.4)
    s.write(b"PARAM SET detection.scalar_onset_threshold 200000\n"); time.sleep(0.4)
    out = [s.readline().decode("ascii", errors="replace").strip() for _ in range(60)]
    ok = any("behavior.idle_enabled" in l and "status=ok" in l for l in out) and any("new=200000" in l for l in out)
    setup_log.append("%s silent %s" % (n, "ok" if ok else "NOT CONFIRMED"))
state_log = []
for n, s in ports.items():
    for cmd in [c.strip() for c in a.setup.split(";") if c.strip()]:
        s.write((cmd + "\n").encode()); time.sleep(0.4)
    for cmd in ("PARAM DUMP",):
        s.write((cmd + "\n").encode()); time.sleep(0.8)
        got = 0
        while True:
            l = s.readline()
            if not l:
                break
            l = l.decode("ascii", errors="replace").strip()
            if l.startswith("PARAM"):
                state_log.append("state %s << %s" % (n, l[:160])); got += 1
        if not got:
            state_log.append("state %s << (no reply to %s)" % (n, cmd))
    s.write(b"RB log full\n")
for l in setup_log:
    print(l, flush=True)

events = []
strengths = {}      # (t, listener) -> strength of a valid verdict
status_heard = {}   # name -> [(t, heard.valid, heard.strength_sum)]
t0 = time.time()
next_status = a.status_every
log_path = os.path.join(a.out, a.name + ".log")
with open(log_path, "w", encoding="utf-8") as f:
    f.write("# nodegroup boards=%s silent=%s secs=%d setup=%s started_unix=%.3f\n" % (a.boards, a.silent, a.secs, a.setup, t0))
    for l in setup_log + state_log:
        f.write("# %s\n" % l)
    next_kick = a.kick_first if a.kick else None
    kicks = []
    while time.time() - t0 < a.secs:
        if next_kick is not None and time.time() - t0 >= next_kick:
            ports[a.kick].write(b"RB CALL\n")
            kicks.append(time.time() - t0)
            f.write("%.2f %s >> RB CALL\n" % (kicks[-1], a.kick))
            next_kick += a.kick_every
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
                m = re.search(r" strength=(\d+)", l)
                if m:
                    strengths[(t, n)] = int(m.group(1))
            elif l.startswith("RB STATUS"):
                m = re.search(r"heard\.valid=(\d+) heard\.strength_sum=(\d+)", l)
                if m:
                    status_heard.setdefault(n, []).append((t, int(m.group(1)), int(m.group(2))))
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
pair_strength = {}  # (listener, sender) -> [(t, strength)]
for t, n, k in events:
    if k != "verdict":
        continue
    # 0.05 s, not 0.15: the PC stamps a verdict 0.13-0.14 s after the sender's "emit start" line (2026-10-21 radio OBS)
    cands = [(te, ne) for te, ne in emits if ne != n and 0.05 <= t - te <= 0.8]
    if cands:
        heard[(n, max(cands)[1])] += 1
        if (t, n) in strengths:
            pair_strength.setdefault((n, max(cands)[1]), []).append((t, strengths[(t, n)]))
    else:
        unattributed += 1
print("  who heard whom (listener <- sender: verdicts / sender's emissions):")
for x in names:
    print("    %s <- %s" % (x, "  ".join("%s %d/%d" % (y, heard[(x, y)], sent[y]) for y in names if y != x)))
print("  unattributed verdicts: %d" % unattributed)
if a.drift_bin > 0 and (pair_strength or status_heard):
    nbins = int(a.secs // a.drift_bin) + (1 if a.secs % a.drift_bin else 0)
    print("  strength drift, bins of %.0f s (median strength, verdict count in brackets):" % a.drift_bin)
    for (x, y) in sorted(pair_strength):
        cells = []
        for b in range(nbins):
            v = sorted(st for t, st in pair_strength[(x, y)] if b * a.drift_bin <= t < (b + 1) * a.drift_bin)
            cells.append("%d(%d)" % (v[len(v) // 2], len(v)) if v else "-")
        allv = sorted(st for _, st in pair_strength[(x, y)])
        print("    %s <- %s: %s  first->last bin %s" % (x, y, " ".join(cells), "%+.0f %%" % (
            100.0 * (int(cells[-1].split("(")[0]) - int(cells[0].split("(")[0])) / int(cells[0].split("(")[0]))
            if cells and cells[0] != "-" and cells[-1] != "-" else "n/a"))
    print("  mean valid strength per board and bin (RB STATUS heard.*; a counter drop = reboot or rebase):")
    for n in sorted(status_heard):
        rows = status_heard[n]
        cells = []
        for b in range(nbins):
            inb = [r for r in rows if b * a.drift_bin <= r[0] < (b + 1) * a.drift_bin]
            prev = [r for r in rows if r[0] < b * a.drift_bin]
            start = prev[-1] if prev else (0.0, 0, 0)  # opening the ports reset the boards: counters from 0
            if not inb or start is None or inb[-1][1] < start[1]:
                cells.append("-")
                continue
            dv, ds = inb[-1][1] - start[1], inb[-1][2] - start[2]
            cells.append("%d(%d)" % (ds // dv, dv) if dv > 0 else "-")
        print("    %s: %s" % (n, " ".join(cells)))
mins = int(a.secs // 60) or 1
per_min = [sum(1 for t, _ in emits if m * 60 <= t < (m + 1) * 60) for m in range(mins)]
print("  group emissions per minute: %s" % " ".join(str(x) for x in per_min))
# Time to quiet after each kick (step 5f, counted along since 2026-10-22, no longer the pass mark): from the kick to the end of the last emission before the group's
# first silence of --quiet-gap s. 'never' when no such silence comes before the next kick or the end.
# An idle call counts as a kick too (owner 2026-10-17): it wakes the network by design.
idle_starts = [t for t, n, k in events if k == "emit_idle"]
starters = sorted(set(round(k, 2) for k in kicks) | set(round(t, 2) for t in idle_starts))
if starters:
    times = sorted(t for t, _ in emits)
    results = []
    kicks = starters
    for i, k in enumerate(kicks):
        limit = kicks[i + 1] if i + 1 < len(kicks) else a.secs
        after = [t for t in times if k <= t < limit]
        quiet_at = None
        prev = k
        for t in after + [limit]:
            if t - prev >= a.quiet_gap:
                quiet_at = prev
                break
            prev = t
        results.append(None if quiet_at is None else quiet_at - k)
        print("  start %d at %.0f s (%s): %s, %d emissions until then" % (
            i + 1, k, "idle" if any(abs(k - t) < 0.01 for t in idle_starts) else "RB CALL", "quiet after %.1f s" % (quiet_at - k) if quiet_at is not None else "never quiet before the next kick",
            sum(1 for t in after if quiet_at is None or t <= quiet_at)))
    done = [r for r in results if r is not None]
    within = sum(1 for r in done if r <= a.quiet_within)
    print("  time to quiet (gap %.0f s): %d of %d starts quiet%s; within %.0f s: %d of %d (pass mark 80 %%: %s)" % (
        a.quiet_gap, len(done), len(results), ", median %.1f s" % sorted(done)[len(done) // 2] if done else "",
        a.quiet_within, within, len(results), "PASS" if results and within >= 0.8 * len(results) else "FAIL"))
# Pass mark since 2026-10-22 (owner; decisions/2026-10-22-network-pacing.md): after a start (RB CALL or idle
# call) the chain ends (a --chain-gap s silence) within --quiet-within s, in 80 % of starts; no minute over M.
# A start inside a chain that is still running is part of that chain, not a new start.
if starters:
    times = sorted(t for t, _ in emits)
    chains = []
    end_prev = -1.0
    for k in starters:
        if k <= end_prev:
            continue
        prev = k
        for t in [t for t in times if t >= k] + [a.secs + a.chain_gap]:
            if t - prev >= a.chain_gap:
                break
            prev = t
        chains.append((k, prev - k, sum(1 for t in times if k <= t <= prev)))
        end_prev = prev
    ended = sum(1 for _, d, _ in chains if d <= a.quiet_within)
    print("  chains: %d distinct starts; length median %.1f s, max %.1f s; ended within %.0f s: %d of %d (80 %%: %s)" % (
        len(chains), sorted(d for _, d, _ in chains)[len(chains) // 2], max(d for _, d, _ in chains), a.quiet_within,
        ended, len(chains), "PASS" if ended >= 0.8 * len(chains) else "FAIL"))
print("  max emissions per minute %d, M = %d: %s" % (max(per_min) if per_min else 0, a.max_per_min,
      "PASS" if (max(per_min) if per_min else 0) <= a.max_per_min else "FAIL"))
print("  log: %s" % log_path)
