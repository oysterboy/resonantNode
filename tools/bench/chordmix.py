"""Chord mix (step 14, docs/research/polyphony.md Part A check 1): sum two or three recorded hop trains and replay the
sum through the real detection chain (native-replay), so "what does a listener report when several Nodes answer at
once" has an answer per level difference, start offset and rotation pair.

Each voice is a RAW capture (mode=both or pcm, tools/bench/rawcorpus.py). The trains are aligned at their capture's
trigger (RAW_BEGIN pre_samples), then voice 2 (and 3) is delayed by --offset-ms and lowered by --gain-db. The mix
keeps voice 1's header and writes only ms,pcm rows, so the replay recomputes every feature from the summed PCM.
Caveat: each recording carries its own room noise, so the mix's floor is ~3 dB (two voices) above one capture's.

  python tools/bench/chordmix.py --replay <native-replay program.exe> --out <folder> \
      --voice A.csv --voice B.csv [--voice C.csv] --gain-db 0,3,6,10 --offset-ms 0,10,30

One line per mix: the verdict, the first failed requirement, the shape facts. --table writes them as CSV.
"""
import argparse
import itertools
import os
import re
import subprocess


def read_capture(path):
    """(header lines before the rows, pre_samples, [pcm])."""
    head, pcm, col = [], [], None
    pre = 0
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.rstrip("\r\n")
        if col is None:
            if line.startswith("ms,"):
                col = line.split(",").index("pcm")
                continue
            if line.startswith("RAW_BEGIN"):
                m = re.search(r"\bpre_samples=(\d+)", line)
                pre = int(m.group(1)) if m else 0
            head.append(line)
        elif line[:1].isdigit():
            pcm.append(int(line.split(",")[col]))
    return head, pre, pcm


def mix(voices, gains_db, offsets_ms, sr=16000):
    """Voice 1 as recorded; voice k > 1 aligned at the trigger, delayed and scaled. The mix runs to the end of the
    last voice (silence after voice 1 ends: a late voice is not cut off)."""
    head, pre0, base = voices[0]
    out = [float(v) for v in base]
    for (h, pre, pcm), g_db, off in zip(voices[1:], gains_db, offsets_ms):
        g = 10.0 ** (-g_db / 20.0)
        shift = pre0 + int(off * sr / 1000) - pre  # index in the mix of this voice's sample 0
        if shift + len(pcm) > len(out):
            out.extend([0.0] * (shift + len(pcm) - len(out)))
        for i, v in enumerate(pcm):
            j = i + shift
            if 0 <= j < len(out):
                out[j] += g * v
    return head, [int(round(v)) for v in out]


def write_mix(path, head, pcm):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        for line in head:
            f.write(line + "\n")
        f.write("ms,pcm\n")
        for i, v in enumerate(pcm):
            f.write("%d,%d\n" % (i // 16, max(-2**31, min(2**31 - 1, v))))


def replay(program, path):
    r = subprocess.run([program, "--verbose", "--shape", path], capture_output=True, text=True)
    out = r.stdout
    v = re.search(r"verdict (.*)", out)
    s = re.search(r"    shape (\S+) .*?distinct=(\d+) up_steps=(\d+) other_steps=(\d+) class=(\w+)", out)
    first = re.search(r"first: (\w+)", out)
    fields = {}
    if v:
        for k in ("reject", "accepted", "valid", "dur_ms", "strength", "failed_target", "observed"):
            m = re.search(r"\b%s=(\S+)" % k, v.group(1))
            fields[k] = m.group(1) if m else ""
    fields["detected"] = "1" if v else "0"
    fields["first"] = first.group(1) if first else "none"
    if s:
        fields.update(distinct=s.group(2), up=s.group(3), other=s.group(4), shape=s.group(5))
    return fields


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--replay", required=True, help="the native-replay program")
    ap.add_argument("--out", required=True, help="folder for the mixed captures")
    ap.add_argument("--voice", action="append", required=True, help="a capture csv; 2 or 3")
    ap.add_argument("--gain-db", default="0", help="voice 2+ below voice 1, dB, comma list")
    ap.add_argument("--offset-ms", default="0", help="voice 2+ after voice 1, ms, comma list")
    ap.add_argument("--table", help="append one CSV row per mix here")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    voices = [read_capture(p) for p in a.voice]
    names = [os.path.splitext(os.path.basename(p))[0] for p in a.voice]
    gains = [float(x) for x in a.gain_db.split(",")]
    offsets = [float(x) for x in a.offset_ms.split(",")]
    rows = []
    for g, off in itertools.product(gains, offsets):
        n = len(voices) - 1
        # voice 3 (if any) at the same level as voice 2 and twice its offset: a spread round
        gains_db = [g] * n
        offsets_ms = [off * (k + 1) for k in range(n)]
        head, pcm = mix(voices, gains_db, offsets_ms)
        label = "%s__%s_g%g_o%g" % (names[0], "+".join(names[1:]), g, off)
        path = os.path.join(a.out, label + ".csv")
        write_mix(path, head, pcm)
        f = replay(a.replay, path)
        row = dict(mix=label, voices=len(voices), gain_db=g, offset_ms=off, **f)
        rows.append(row)
        print("%-58s acc=%s valid=%s %-22s dur=%-4s str=%-6s failed=%-8s shape=%s/%s/%s %s" % (
            label, f.get("accepted", "-"), f.get("valid", "-"), f.get("reject", "no_occurrence"), f.get("dur_ms", ""),
            f.get("strength", ""), f.get("failed_target", ""), f.get("distinct", "-"), f.get("up", "-"),
            f.get("other", "-"), f.get("shape", "")), flush=True)
    if a.table:
        keys = ["mix", "voices", "gain_db", "offset_ms", "detected", "first", "accepted", "valid", "reject",
                "failed_target", "observed", "dur_ms", "strength", "distinct", "up", "other", "shape"]
        new = not os.path.exists(a.table)
        with open(a.table, "a", encoding="utf-8", newline="\n") as t:
            if new:
                t.write(",".join(keys) + "\n")
            for r in rows:
                t.write(",".join(str(r.get(k, "")) for k in keys) + "\n")


if __name__ == "__main__":
    main()
