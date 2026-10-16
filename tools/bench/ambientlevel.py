"""Ambient level at the mic, from RAW i2s captures: broadband RMS and band energy.

Used to match the playback level of a test audio file to a reference source
(radio) by the energy in the detection band (default 2800-3800 Hz), not by ear.
Each capture is a short RAW trigger (the Emitter plays a 20 ms tone first; the
first 80 ms after emit_done are skipped), the rest is ambient sound. Powers are
averaged over the captures and reported in dB re full scale (24 bit).

  python tools/bench/ambientlevel.py --port COM5 --out bench/sessions/<folder> --label wdr5_music --reps 12
"""
import argparse, math, os, re, sys, time
import serial

ap = argparse.ArgumentParser()
ap.add_argument("--port", required=True)
ap.add_argument("--out", required=True, help="session folder; csv files and ambient_<label>.txt are written there")
ap.add_argument("--label", required=True)
ap.add_argument("--reps", type=int, default=12)
ap.add_argument("--band", default="2800-3800")
ap.add_argument("--post-ms", type=int, default=340)
ap.add_argument("--skip-ms", type=int, default=80)
a = ap.parse_args()
lo, hi = (int(x) for x in a.band.split("-"))
os.makedirs(a.out, exist_ok=True)
s = serial.Serial(a.port, 115200, timeout=0.05)
FS = float(1 << 23)


def pump(t, until=None):
    lines, end = [], time.time() + t
    while time.time() < end:
        l = s.readline()
        if l:
            l = l.decode(errors="replace").rstrip()
            lines.append(l)
            if until and until in l:
                break
    return lines


def goertzel_power(x, sr, f):
    w = 2 * math.pi * f / sr
    c = 2 * math.cos(w)
    s1 = s2 = 0.0
    for v in x:
        s0 = v + c * s1 - s2
        s2, s1 = s1, s0
    return (s1 * s1 + s2 * s2 - c * s1 * s2) / (len(x) * len(x) / 4.0)  # amplitude^2


pump(3.5)
tot, band_p, n = [], [], 0
for rep in range(a.reps):
    s.write(("RAW trigger f=3200 dur=20 post=%d mode=i2s\n" % a.post_ms).encode())
    lines = pump(12, "RAW_END")
    open(os.path.join(a.out, "ambient_%s_r%d.csv" % (a.label, rep)), "w").write("\n".join(lines))
    sr, done = 16000, None
    rows = []
    for l in lines:
        m = re.search(r"sr=(\d+).*emit_done_ms=(\d+)", l)
        if m:
            sr, done = int(m[1]), int(m[2])
        if re.match(r"^\d+,-?\d", l):
            p = l.split(",")
            try:
                rows.append((int(p[0]), float(p[1])))
            except (ValueError, IndexError):
                pass
    if not rows or done is None:
        print("rep %d: no data" % rep)
        continue
    seg = [v for ms, v in rows if ms >= done + a.skip_ms]
    if len(seg) < 1024:
        print("rep %d: only %d samples after skip" % (rep, len(seg)))
        continue
    mean = sum(seg) / len(seg)
    seg = [v - mean for v in seg]
    rms2 = sum(v * v for v in seg) / len(seg)
    ps = [goertzel_power(seg, sr, f) for f in range(lo, hi + 1, 100)]
    bp = sum(ps) / len(ps) / 2.0  # mean tone power per bin -> noise-like power per component
    tot.append(rms2)
    band_p.append(bp)
    n += 1
    print("rep %2d: %5d samples  rms %.1f dBFS  band %.1f dBFS" % (
        rep, len(seg), 10 * math.log10(rms2 / FS / FS + 1e-30), 10 * math.log10(bp / FS / FS + 1e-30)))

if n:
    mt, mb = sum(tot) / n, sum(band_p) / n
    line = "%s: %d captures  rms %.1f dBFS  band %d-%d Hz %.1f dBFS  band/rms %.1f dB" % (
        a.label, n, 10 * math.log10(mt / FS / FS), lo, hi, 10 * math.log10(mb / FS / FS), 10 * math.log10(mb / mt))
    print(line)
    open(os.path.join(a.out, "ambient_%s.txt" % a.label), "w").write(line + "\n")
