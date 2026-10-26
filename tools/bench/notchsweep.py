#!/usr/bin/env python3
"""Replay sweep over a hop capture with one, then two band bins notched (step 5h, assumption 2).

The 5-step hop train survived one comb null per spacing on the bench (5b); two nulls never
occurred there. This filters the capture with a peaking-EQ notch (RBJ biquad, depth 1/3 or 1/7
in amplitude, bandwidth --bw-hz) at one bin, then at every pair of bins, and replays each
variant through the real detection chain (.pio/build/native-replay/program.exe) on the shipped
profile. Per variant it reports acceptance, the reject's failed target, the occurrence duration
and the contrast P75 the inspector saw. The P75 is not printed by the replay build; it is
bisected: the largest contrast_medium at which the occurrence is still accepted is the P75
(the inspector accepts P75 >= medium). Reported as '<10000' below the weak threshold, '-' when
the detector forms no occurrence at all.

  python tools/bench/notchsweep.py test/fixtures/hop5_25x5g10_70cm_2026-10-16.csv --depth 3,7
"""
import argparse
import itertools
import math
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hopvariants import field, load  # noqa: E402

PROGRAM = os.path.join(".pio", "build", "native-replay", "program.exe")
CONTRAST_WEAK = 10000
CONTRAST_STRONG = 300000
SHIPPED_MEDIUM = 30500


def peaking(pcm, sr, f0, gain, q):
    a_ = math.sqrt(gain)  # A = 10^(dB/40) = sqrt(amplitude gain)
    w0 = 2 * math.pi * f0 / sr
    alpha = math.sin(w0) / (2 * q)
    b0, b1, b2 = 1 + alpha * a_, -2 * math.cos(w0), 1 - alpha * a_
    a0, a1, a2 = 1 + alpha / a_, -2 * math.cos(w0), 1 - alpha / a_
    b0, b1, b2, a1, a2 = b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0
    out, x1, x2, y1, y2 = [], 0.0, 0.0, 0.0, 0.0
    for x in pcm:
        y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, x, y1, y
        out.append(int(round(y)))
    return out


def write(path, head, begin, summary, pcm, tag):
    per_ms = int(field(begin, "sr")) // 1000
    t0 = int(field(begin, "command_ms") or 0)
    b = re.sub(r"\bfields=\S+", "fields=ms,pcm", begin)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        for h in head:
            if h.startswith("RAW_INFO"):
                f.write(h + "\n")
        f.write(b + " synthetic=%s\n" % tag)
        f.write("ms,pcm\n")
        for i, v in enumerate(pcm):
            f.write("%d,%d\n" % (t0 + i // per_ms, v))
        f.write("RAW_SUMMARY id=1 captured=%d synthetic=%s baseline=%s\n" % (len(pcm), tag, field(summary, "baseline")))


def run(path, medium=None):
    cmd = [PROGRAM, "--verbose"]
    if medium is not None:
        cmd += ["--set", "detection.contrast_medium=%d" % medium]
    proc = subprocess.run(cmd + [path], capture_output=True, text=True)
    if proc.returncode != 0:
        # 0xC0000135 on Windows: the replay's MinGW runtime DLLs are not on PATH (CLAUDE.md, host build).
        sys.exit("replay failed (exit %d) on %s: is the host compiler's bin directory on PATH?" % (proc.returncode, path))
    out = proc.stdout
    verdicts = [l for l in out.splitlines() if l.strip().startswith("verdict ")]
    return verdicts


def accepted(verdicts):
    return any(" accepted=1 " in v for v in verdicts)


def p75(path):
    if accepted(run(path, CONTRAST_STRONG - 1)):
        return ">=%d" % CONTRAST_STRONG
    if not accepted(run(path, CONTRAST_WEAK)):
        return "<%d" % CONTRAST_WEAK
    lo, hi = CONTRAST_WEAK, CONTRAST_STRONG - 1  # accepted at lo, not at hi
    while hi - lo > 50:
        mid = (lo + hi) // 2
        if accepted(run(path, mid)):
            lo = mid
        else:
            hi = mid
    return "%d" % lo


def describe(path):
    v = run(path)
    if not v:
        return "no", "detector", "-", "-"
    first = next((x for x in v if " accepted=1 " in x), v[0])
    dur = re.search(r"dur_ms=(\d+)", first).group(1)
    ok = " accepted=1 " in first
    reason = "-" if ok else (re.search(r"failed_target=(\S+)", first) or re.search(r"reject=(\S+)", first)).group(1)
    return ("yes" if ok else "no"), reason, dur, p75(path)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("capture")
    ap.add_argument("--depth", default="3,7", help="amplitude divisors of the notch")
    ap.add_argument("--bw-hz", type=float, default=125.0, help="notch bandwidth (Q = f / bw); bins are 250 Hz apart")
    ap.add_argument("--no-pairs", dest="pairs", action="store_false", help="single bins only")
    ap.add_argument("--keep", default="", help="directory to keep the variants in (default: a temp dir)")
    a = ap.parse_args()
    if not os.path.exists(PROGRAM):
        sys.exit("build the replay first: pio run -e native-replay")

    head, begin, summary, pcm = load(a.capture)
    sr = int(field(begin, "sr"))
    bins = [int(x) for x in field(begin, "hop").split(",")]
    work = a.keep or tempfile.mkdtemp(prefix="notch_")
    os.makedirs(work, exist_ok=True)

    print("capture %s, bins %s, notch bandwidth %.0f Hz, shipped contrast_medium %d" % (
        os.path.basename(a.capture), ",".join(map(str, bins)), a.bw_hz, SHIPPED_MEDIUM))
    print()
    print("| notched | depth | accepted | failed | dur_ms | contrast P75 | margin to %d |" % SHIPPED_MEDIUM)
    print("|---|---|---|---|---|---|---|")

    def row(label, depth, path):
        acc, reason, dur, p = describe(path)
        margin = str(int(p) - SHIPPED_MEDIUM) if p.isdigit() else "-"
        print("| %s | %s | %s | %s | %s | %s | %s |" % (label, depth, acc, reason, dur, p, margin))
        sys.stdout.flush()

    ref = os.path.join(work, "ref.csv")
    write(ref, head, begin, summary, pcm, "ref")
    row("none", "-", ref)
    sets = [(b,) for b in bins] + (list(itertools.combinations(bins, 2)) if a.pairs else [])
    for d in [int(x) for x in a.depth.split(",")]:
        for s in sets:
            x = pcm
            for f0 in s:
                x = peaking(x, sr, f0, 1.0 / d, f0 / a.bw_hz)
            tag = "notch_%s_x%d" % ("+".join(map(str, s)), d)
            path = os.path.join(work, tag + ".csv")
            write(path, head, begin, summary, x, tag)
            row("+".join(map(str, s)), "1/%d" % d, path)


if __name__ == "__main__":
    main()
