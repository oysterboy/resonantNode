"""False alarms: what the room alone makes the nodes detect, with no node calling, per preset, in quiet and in noise.

  python tools/bench/falsealarm.py --gateway E0=COM12 --nodes R1,E2 --presets trill,cricket --window 2500-3500 \
      --block 1200 --noise C:/Users/admin/Downloads/bench_audio.wav --out bench/sessions/<folder>

Every node gets the preset and the window, RB log full and the radio log, and is muted (output.silent 1: it still
detects) with its idle call off, so whatever it detects came from the room. Per preset two blocks: quiet, then the
noise file looped through the lab PC's default output (Windows SoundPlayer, the device's own volume; --noise none
skips it; --conditions noise runs the noise blocks only). The presets alternate, reversed in the second round
(--rounds), so a drift of the room does not land on one preset. At the end every node stays muted with its idle call back on (live; a reboot loads the stored setup).

Counted per node and block from the radio log: valid verdicts (a false alarm: behavior would answer it), invalid
verdicts (with the failed check), detector rejects (with the reason), and from the once-a-second RB level lines the
loudest input and the seconds it reached the onset threshold: a noise block that never reaches it tested nothing. Writes block_<n>_<preset>_<quiet|noise>.log,
summary.csv (one row per preset, condition and node, with false alarms per 10 minutes) and changes.txt.
Needs a Node image with the RB reject / inspect / level lines (notes_manual.md, "Hearing test").
"""
import argparse
import collections
import csv
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nodepreset  # noqa: E402
from hearingtest import LOG, field, listen_cmds  # noqa: E402


def start_noise(path):
    # PlayLooping returns at once; the PowerShell process holds the player until it is killed.
    ps = "$p = New-Object System.Media.SoundPlayer '%s'; $p.PlayLooping(); Start-Sleep -Seconds 86400" % path
    return subprocess.Popen(["powershell", "-NoProfile", "-Command", ps])


def count(path, nodes):
    c = {n: collections.Counter() for n in nodes}
    for line in open(path, encoding="utf-8"):
        p = line.rstrip("\n").split(" ", 2)
        if len(p) < 3 or p[1] not in c:
            continue
        who, text = p[1], p[2]
        if text.startswith("RB verdict="):
            if "valid=1" in text:
                c[who]["valid"] += 1
            else:
                c[who]["invalid"] += 1
                c[who]["invalid " + (field(text, "failed") or field(text, "reject") or "?")] += 1
        elif text.startswith("RB level "):
            peak = int(field(text, "peak") or 0)
            c[who]["peak"] = max(c[who]["peak"], peak)
            c[who]["over_onset_s"] += 1 if peak >= int(field(text, "onset") or 1 << 30) else 0
        elif text.startswith("RB reject "):
            c[who]["reject"] += 1
            c[who]["reject " + (field(text, "reason") or "?")] += 1
    return c


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gateway", required=True)
    ap.add_argument("--nodes", required=True)
    ap.add_argument("--presets", required=True, help="comma-separated, e.g. trill,cricket")
    ap.add_argument("--window", required=True, help="low-high Hz")
    ap.add_argument("--block", type=int, default=1200, help="seconds per preset and condition")
    ap.add_argument("--rounds", type=int, default=1)
    ap.add_argument("--noise", default="none", help="a .wav looped during the noise blocks, or none")
    ap.add_argument("--listen", choices=["own", "band"], default="own",
                    help="own: each node listens on its own bins (behavior.band_per_node 1, as many bins as its call "
                    "has steps); band: every node listens on the whole window (band_per_node 0, detection.band_*)")
    ap.add_argument("--conditions", default="quiet,noise",
                    help="which blocks per preset: quiet,noise (default), or one of them, e.g. noise for a louder rerun")
    ap.add_argument("--settle", type=int, default=8)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    gw = nodepreset.parse(a.gateway)[0]
    nodes = [n for n in a.nodes.split(",") if n]
    presets = [p for p in a.presets.split(",") if p]
    lo, hi = a.window.split("-")
    conditions = [c for c in a.conditions.split(",") if c and not (c == "noise" and a.noise == "none")]

    def say(text):
        line = "%s %s" % (time.strftime("%H:%M:%S"), text)
        print(line, flush=True)
        with open(os.path.join(a.out, "changes.txt"), "a", encoding="utf-8") as f:
            f.write(line + "\n")

    tally = collections.defaultdict(collections.Counter)
    seconds = collections.Counter()
    block = 0
    for r in range(a.rounds):
        for preset in (presets if r % 2 == 0 else presets[::-1]):
            _, cmds = nodepreset.read_preset(preset)
            nodepreset.apply_radio(gw, nodes, cmds + listen_cmds(a.listen, lo, hi) + [
                "PARAM SET node.radio_log 20", "RB log full", "PARAM SET behavior.call_enabled 0",
                "PARAM SET output.silent 1"])
            for cond in conditions:
                block += 1
                say("block %d round %d: %s, %s, window %s-%s Hz, nobody calling" % (block, r + 1, preset, cond, lo, hi))
                time.sleep(a.settle)
                player = start_noise(a.noise) if cond == "noise" else None
                path = os.path.join(a.out, "block_%02d_%s_%s.log" % (block, preset, cond))
                s = nodepreset.open_port(gw[1], timeout=0.05)
                t0, buf = time.time(), b""
                try:
                    with open(path, "w", encoding="utf-8") as f:
                        while time.time() - t0 < a.block:
                            chunk = s.read(4096)
                            if not chunk:
                                continue
                            buf += chunk
                            while b"\n" in buf:
                                raw, buf = buf.split(b"\n", 1)
                                m = LOG.match(raw.decode("ascii", errors="replace").strip())
                                if m:
                                    f.write("%.2f %s %s\n" % (time.time() - t0, m.group(1), m.group(2)))
                finally:
                    s.close()
                    if player:
                        player.kill()
                c = count(path, nodes)
                for n in nodes:
                    peak = max(tally[(preset, cond, n)]["peak"], c[n]["peak"])
                    tally[(preset, cond, n)].update(c[n])
                    tally[(preset, cond, n)]["peak"] = peak
                    seconds[(preset, cond, n)] += a.block
                say("  " + "  ".join("%s valid=%d invalid=%d reject=%d peak=%d loud_s=%d" % (
                    n, c[n]["valid"], c[n]["invalid"], c[n]["reject"], c[n]["peak"], c[n]["over_onset_s"]) for n in nodes))
                write_summary(a.out, tally, seconds)
    nodepreset.apply_radio(gw, nodes, ["PARAM SET output.silent 1", "PARAM SET behavior.call_enabled 1"])
    say("done: every node muted, idle call back on (live)")
    for line in open(os.path.join(a.out, "summary.csv"), encoding="utf-8"):
        print(line.rstrip())


def write_summary(out, tally, seconds):
    with open(os.path.join(out, "summary.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["preset", "condition", "node", "minutes", "valid", "valid_per_10min", "invalid", "reject",
                    "peak", "seconds_over_onset", "details"])
        for key in sorted(tally):
            c, mins = tally[key], seconds[key] / 60.0
            detail = "; ".join("%s x%d" % (k, v) for k, v in c.most_common()
                               if k.startswith("invalid ") or k.startswith("reject "))
            w.writerow(list(key) + ["%.0f" % mins, c["valid"], "%.1f" % (10 * c["valid"] / mins) if mins else "",
                                    c["invalid"], c["reject"], c["peak"], c["over_onset_s"], detail])


if __name__ == "__main__":
    main()
