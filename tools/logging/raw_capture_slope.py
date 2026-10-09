#!/usr/bin/env python3
"""Classify the slow drift in a RAW capture: mic low-frequency content, or
something integrating the stream.

Usage:
    python3 tools/logging/raw_capture_slope.py <serial-log> [--tone-hz 3200]

Feed it the serial log of one `RAW ... mode=i2s` capture (the i2s mode reads
decoded words straight from the driver, so it bypasses preprocessSample() and
shows the un-differenced stream with today's firmware). mode=pcm logs work
too but show the differenced stream when FirstDifference is the default.

It reports, for the pre-trigger (quiet) rows when there are enough, else all:
  dc            mean of the stream (PCM)
  drift         least-squares slope (PCM per second)
  band levels   RMS per octave band, raw and first-differenced, in dB
  slope         dB per octave of the raw noise floor between 1 kHz and 7 kHz,
                with the tone band excluded

Reading the slope: a healthy MEMS mic has a flat noise floor from a few
hundred Hz to Nyquist (slope near 0 dB/oct), with its DC offset and 1/f or
infrasound content only below that. Integrated white noise falls 6 dB per
octave across the whole band. So:
  slope near  0  ->  the drift is low-frequency mic output; a DC blocker /
                     high-pass fixes it, nothing in the pipeline integrates
  slope near -6  ->  something integrates between mic and decode
Pure Python, no dependencies; numpy is used when present.
"""
import math
import sys


def parse_capture(path):
    rows = []
    header = {}
    in_rows = False
    fields = None
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if line.startswith("RAW_BEGIN"):
                for tok in line.split()[1:]:
                    if "=" in tok:
                        k, v = tok.split("=", 1)
                        header[k] = v
                in_rows = True
                fields = None
                rows = []
                continue
            if line.startswith("RAW_SUMMARY"):
                in_rows = False
                continue
            if not in_rows:
                continue
            if fields is None:
                if line.startswith("ms,"):
                    fields = line.split(",")
                continue
            parts = line.split(",")
            if len(parts) != len(fields):
                continue
            try:
                rows.append({k: float(v) for k, v in zip(fields, parts)})
            except ValueError:
                continue
    if not rows or "pcm" not in fields:
        raise SystemExit("no RAW_BEGIN ... ms,pcm rows found in %s" % path)
    return header, fields, rows


def fft(x):
    n = len(x)
    if n == 1:
        return [complex(x[0])]
    even = fft(x[0::2])
    odd = fft(x[1::2])
    out = [0j] * n
    for k in range(n // 2):
        t = complex(math.cos(-2 * math.pi * k / n), math.sin(-2 * math.pi * k / n)) * odd[k]
        out[k] = even[k] + t
        out[k + n // 2] = even[k] - t
    return out


def power_spectrum(x, sr):
    n = 1
    while n * 2 <= len(x):
        n *= 2
    x = x[:n]
    mean = sum(x) / n
    win = [0.5 - 0.5 * math.cos(2 * math.pi * i / (n - 1)) for i in range(n)]
    xw = [(v - mean) * w for v, w in zip(x, win)]
    try:
        import numpy as np
        spec = np.fft.rfft(np.array(xw))
        mags = (np.abs(spec) ** 2).tolist()
    except ImportError:
        spec = fft(xw)
        mags = [abs(c) ** 2 for c in spec[: n // 2 + 1]]
    freqs = [i * sr / n for i in range(len(mags))]
    return freqs, mags


def band_db(freqs, mags, lo, hi, exclude=None):
    acc = 0.0
    cnt = 0
    for f, m in zip(freqs, mags):
        if f < lo or f >= hi:
            continue
        if exclude and exclude[0] <= f <= exclude[1]:
            continue
        acc += m
        cnt += 1
    if cnt == 0 or acc <= 0:
        return float("nan")
    return 10 * math.log10(acc / cnt)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    path = argv[1]
    tone = 3200.0
    if "--tone-hz" in argv:
        tone = float(argv[argv.index("--tone-hz") + 1])
    header, fields, rows = parse_capture(path)
    sr = float(header.get("sr", 16000))
    trigger_ms = float(header.get("trigger_ms", "nan"))

    pcm = [r["pcm"] for r in rows]
    pre = [r["pcm"] for r in rows if not math.isnan(trigger_ms) and r["ms"] < trigger_ms]
    series = pre if len(pre) >= 512 else pcm
    label = "pre-trigger" if series is pre else "all"
    n = len(series)

    dc = sum(series) / n
    t = [i / sr for i in range(n)]
    tm = sum(t) / n
    sxx = sum((ti - tm) ** 2 for ti in t)
    sxy = sum((ti - tm) * (v - dc) for ti, v in zip(t, series))
    slope_pcm_s = sxy / sxx if sxx else float("nan")
    rms = math.sqrt(sum((v - dc) ** 2 for v in series) / n)

    diff = [(b - a) / 2.0 for a, b in zip(series, series[1:])]

    print("capture   : %s  rows=%d  sr=%g  window=%s (%d samples)" % (path, len(rows), sr, label, n))
    print("dc        : %.1f PCM" % dc)
    print("drift     : %.1f PCM/s (least-squares over %.2f s)" % (slope_pcm_s, n / sr))
    print("rms       : raw %.1f   first-diff/2 %.1f" % (rms, math.sqrt(sum(d * d for d in diff) / len(diff))))

    fr, mr = power_spectrum(series, sr)
    fd, md = power_spectrum(diff, sr)
    excl = (tone - 250, tone + 250)
    bands = [(125, 250), (250, 500), (500, 1000), (1000, 2000), (2000, 4000), (4000, 8000)]
    print("band Hz        raw dB   diff dB")
    for lo, hi in bands:
        print("%5d-%-5d  %8.1f  %8.1f" % (lo, hi, band_db(fr, mr, lo, hi, excl), band_db(fd, md, lo, hi, excl)))

    lo_db = band_db(fr, mr, 1000, 2000, excl)
    hi_db = band_db(fr, mr, 4000, 7000, excl)
    octaves = math.log2(5500.0 / 1500.0)
    slope = (hi_db - lo_db) / octaves
    print("slope 1k->7k : %.1f dB/oct (tone band %g..%g Hz excluded)" % (slope, excl[0], excl[1]))
    if slope > -3:
        print("verdict   : flat floor. The drift is mic low-frequency output (DC offset, 1/f, infrasound);")
        print("            nothing in the pipeline integrates. Fix shape: DC blocker / high-pass, not a differentiator.")
    else:
        print("verdict   : falling floor, consistent with an integrated stream. Something between mic and")
        print("            decode accumulates; see docs/refactors/i2s-first-difference-revisit.md section 8.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
