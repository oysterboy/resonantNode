"""Independent check of the received tone: RAW captures at several frequencies.

Triggers the Emitter through the Analyzer (RAW trigger f=.. mode=i2s), parses the
CSV, and reports per frequency the tone amplitude (single-bin DFT at f over the
emit window) and the length of the 5 ms RMS envelope above half its peak. The
detector is not involved, so this separates "the received signal is weak/short"
from "the detector rejects it".

  python tools/bench/rawtone.py --port COM3 --out bench/sessions/<folder> 3000 3100 3200
"""
import argparse, math, os, re, sys, time
import serial

ap = argparse.ArgumentParser()
ap.add_argument("--port", required=True)
ap.add_argument("--out", required=True, help="session folder; raw csv files are written there")
ap.add_argument("--dur", type=int, default=100)
ap.add_argument("--reps", type=int, default=2)
ap.add_argument("--mode", default="i2s")
ap.add_argument("freqs", nargs="+", type=int)
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)
s = serial.Serial(a.port, 115200, timeout=0.05)

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

pump(3.5)
print("freq_hz rep  tone_amp   env_len_ms  peak_rms")
for f in a.freqs:
    for rep in range(a.reps):
        s.write(f"RAW trigger f={f} dur={a.dur} post=200 mode={a.mode}\n".encode())
        lines = pump(12, "RAW_END")
        open(os.path.join(a.out, f"raw_f{f}_r{rep}.csv"), "w").write("\n".join(lines))
        col, rows, sr, t0, t1 = None, [], 16000, None, None
        for l in lines:
            m = re.search(r"sr=(\d+).*emit_start_ms=(\d+) emit_done_ms=(\d+)", l)
            if m: sr, t0, t1 = int(m[1]), int(m[2]), int(m[3])
            if l.startswith("ms,"):
                col = l.split(",").index("centered" if "centered" in l else l.split(",")[1]); continue
            if col is not None and re.match(r"^\d+,-?\d", l):
                p = l.split(",")
                try: rows.append((int(p[0]), float(p[col])))
                except (ValueError, IndexError): pass
        if not rows or t0 is None:
            print(f"{f:7d} {rep:3d}  no data"); continue
        ys = [v for _, v in rows]
        # window: emit start .. emit_done
        idx = [i for i, (ms, _) in enumerate(rows) if t0 <= ms <= t1]
        seg = [ys[i] for i in idx] or ys
        w = 2 * math.pi * f / sr
        re_ = sum(v * math.cos(w * n) for n, v in enumerate(seg))
        im_ = sum(v * math.sin(w * n) for n, v in enumerate(seg))
        amp = 2 * math.hypot(re_, im_) / max(len(seg), 1)
        # 5 ms RMS envelope over the whole capture, samples are in order
        n5 = sr // 200
        env = [math.sqrt(sum(x * x for x in ys[i:i + n5]) / n5) for i in range(0, len(ys) - n5, n5)]
        pk = max(env) if env else 0
        ln = sum(1 for e in env if e > pk / 2) * 5
        print(f"{f:7d} {rep:3d}  {amp:9.0f}  {ln:9d}  {pk:9.0f}")
