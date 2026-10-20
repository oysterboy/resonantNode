#!/usr/bin/env python3
"""Hop train variants from one RAW capture, for the host replay (step 5h, assumption 1).

The 70 cm hop capture (test/fixtures/hop5_25x5g10_70cm_2026-10-16.csv) is one room: its tail fills
the 10 ms gaps between the five steps. This writes variants of it that say whether the detector
bridges the gaps by itself or only with that tail:

  dry_gN  the five step windows as recorded, everything else (gaps and the tail after the train)
          replaced by the capture's own pre-roll noise; gaps of N ms
  wet_gN  the capture as recorded, each 10 ms gap (with its tail) followed by N - 10 ms of pre-roll
          noise: a room whose tail is there but whose gaps are longer

A step window still holds the previous step's tail (it overlaps in time); "dry" removes the tail
in the gaps only. Step windows come from the send plan, not from the envelope (reverb hides the
gaps there): onset + k * (pulse + gap), pulse ms long. The onset is the first sample whose
1 ms RMS reaches --onset-fraction of the train's peak 1 ms RMS (or --onset-ms).

Output: RAW transcripts in the replay's format (fields=ms,pcm, mode and preprocess kept, gap= set
to N, RAW_SUMMARY with the source's baseline so the replay seeds the board's AudioSignal baseline).

  python tools/bench/hopvariants.py test/fixtures/hop5_25x5g10_70cm_2026-10-16.csv \
      --dry 10,15,20,30 --wet 15,20,30 --out-dir test/fixtures
"""
import argparse
import math
import os
import re
import sys


def load(path):
    head, rows, summary = [], [], None
    begin = None
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\r\n")
        if line.startswith("RAW_BEGIN "):
            begin = line
        elif line.startswith("RAW_SUMMARY "):
            summary = line
        elif line.startswith("RAW_INFO") or line.startswith("RAW_EMIT"):
            head.append(line)
        elif line[:1].isdigit():
            parts = line.split(",")
            rows.append(int(parts[-1]))  # pcm is the last field in every RAW output (ms,pcm / ms,amp,pcm)
    if begin is None or summary is None:
        sys.exit("%s: no RAW_BEGIN / RAW_SUMMARY" % path)
    return head, begin, summary, rows


def field(line, key):
    m = re.search(r"\b%s=(\S+)" % key, line)
    return m.group(1) if m else None


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("capture")
    ap.add_argument("--dry", default="10,15,20,30", help="gap lengths (ms) of the dry variants, '' for none")
    ap.add_argument("--wet", default="15,20,30", help="gap lengths (ms) of the wet variants, '' for none")
    ap.add_argument("--onset-ms", type=float, default=None, help="train onset in ms from the first sample")
    ap.add_argument("--onset-fraction", type=float, default=0.2)
    ap.add_argument("--noise-ms", type=float, default=30.0, help="pre-roll length used as the noise source")
    ap.add_argument("--out-dir", default=".")
    a = ap.parse_args()

    head, begin, summary, pcm = load(a.capture)
    sr = int(field(begin, "sr"))
    pulse_ms = int(field(begin, "dur"))
    gap_ms = int(field(begin, "gap"))
    steps = int(field(begin, "pulses"))
    baseline = field(summary, "baseline")
    per_ms = sr // 1000

    def rms_ending_at(i):
        seg = pcm[max(0, i - per_ms + 1):i + 1]
        return math.sqrt(sum(x * x for x in seg) / len(seg))

    if a.onset_ms is not None:
        onset = int(round(a.onset_ms * per_ms))
    else:
        env = [rms_ending_at(i) for i in range(len(pcm))]
        peak = max(env)
        onset = next(i for i, v in enumerate(env) if v >= a.onset_fraction * peak) - per_ms + 1
    noise = pcm[:int(a.noise_ms * per_ms)]
    if onset <= len(noise):
        sys.exit("onset %d is inside the noise pre-roll" % onset)
    pulse = pulse_ms * per_ms
    period = (pulse_ms + gap_ms) * per_ms
    windows = [(onset + k * period, onset + k * period + pulse) for k in range(steps)]
    train_end = windows[-1][1]
    print("%s: %d samples, onset %.1f ms, steps at %s ms" % (
        os.path.basename(a.capture), len(pcm), onset / per_ms, ", ".join("%.0f" % (s / per_ms) for s, _ in windows)))

    noise_pos = [0]

    def fill(n):
        out = []
        for _ in range(n):
            out.append(noise[noise_pos[0] % len(noise)])
            noise_pos[0] += 1
        return out

    stem = os.path.splitext(os.path.basename(a.capture))[0]

    def write(kind, g, samples):
        name = os.path.join(a.out_dir, "%s_%s_g%d.csv" % (stem, kind, g))
        b = re.sub(r"\bgap=\d+", "gap=%d" % g, begin)
        b = re.sub(r"\bfields=\S+", "fields=ms,pcm", b)
        b = re.sub(r"\boutput=csv\b", "output=csv synthetic=%s_g%d source=%s" % (kind, g, os.path.basename(a.capture)), b, count=1)
        with open(name, "w", encoding="utf-8", newline="\n") as f:
            f.write("# hopvariants.py: %s variant, gap %d ms, of %s (onset %.1f ms)\n" % (kind, g, os.path.basename(a.capture), onset / per_ms))
            for h in head:
                if h.startswith("RAW_INFO"):
                    f.write(h + "\n")
            f.write(b + "\n")
            f.write("ms,pcm\n")
            t0 = int(field(begin, "command_ms") or 0)
            for i, v in enumerate(samples):
                f.write("%d,%d\n" % (t0 + i // per_ms, v))
            f.write("RAW_SUMMARY id=1 captured=%d synthetic=%s_g%d baseline=%s\n" % (len(samples), kind, g, baseline))
        print("  %-8s %d samples -> %s" % ("%s_g%d" % (kind, g), len(samples), name))

    tail_after = len(pcm) - train_end  # keep the capture's length after the train
    for g in [int(x) for x in a.dry.split(",") if x.strip()]:
        noise_pos[0] = 0
        out = list(pcm[:onset])
        for k, (s, e) in enumerate(windows):
            out += pcm[s:e]
            if k + 1 < steps:
                out += fill(g * per_ms)
        out += fill(tail_after)
        write("dry", g, out)
    for g in [int(x) for x in a.wet.split(",") if x.strip()]:
        if g < gap_ms:
            sys.exit("a wet variant cannot shorten the gap (%d < %d)" % (g, gap_ms))
        noise_pos[0] = 0
        out = list(pcm[:onset])
        for k, (s, e) in enumerate(windows):
            out += pcm[s:e]
            if k + 1 < steps:
                out += pcm[e:e + gap_ms * per_ms] + fill((g - gap_ms) * per_ms)
        out += pcm[train_end:]
        write("wet", g, out)


if __name__ == "__main__":
    main()
