"""Capture corpus: RAW captures (PCM, with the detector's amp feature) as CSV + WAV.

One capture = one RAW trigger on the Analyzer (the Emitter plays a plain tone over
the control UART; hop/sweep need the RAW command to forward them first). The serial
transcript is kept as <label>_r<n>.csv (the format rawtone.py and ambientlevel.py
already use), the pcm column is also written as <label>_r<n>.wav (16 kHz, mono,
24-bit) for an offline replay, and captures.json in the session folder lists
every capture with its RAW_BEGIN fields, the firmware banner and a tone check.

Capture length is limited by the firmware: pre + post <= 6000 samples (375 ms).

  signal:  python tools/bench/rawcorpus.py --port COM5 --out bench/sessions/<folder> \
             --label chirp3200_pos0 --distance-cm 90 --reps 3
  noise:   python tools/bench/rawcorpus.py --port COM5 --out bench/sessions/<folder> \
             --label radio_music --dur 20 --post 340 --reps 12
           (the 20 ms marker tone and its ~80 ms tail sit at the start; skip them)
  convert: python tools/bench/rawcorpus.py --from-csv bench/sessions/2026-10-14-up-90cm-rawscan
           (no board; writes a .wav next to every RAW csv and captures.json)
"""
import argparse, glob, json, math, os, re, struct, sys, time

ap = argparse.ArgumentParser()
ap.add_argument("--port")
ap.add_argument("--out", help="session folder; csv, wav and captures.json are written there")
ap.add_argument("--label", default="capture")
ap.add_argument("--reps", type=int, default=3)
ap.add_argument("--freq", type=int, default=3200)
ap.add_argument("--dur", type=int, default=100, help="emitted tone, ms")
ap.add_argument("--pre", type=int, default=50, help="ms kept before the trigger")
ap.add_argument("--post", type=int, default=250, help="ms captured from the trigger")
ap.add_argument("--mode", default="both", help="both (pcm + amp feature), i2s, pcm")
ap.add_argument("--distance-cm", type=float)
ap.add_argument("--orientation", default="mic and speaker facing up, bench")
ap.add_argument("--note", default="")
ap.add_argument("--from-csv", help="folder (or one csv): convert existing RAW csv files, no board")
a = ap.parse_args()

if a.pre + a.post > 375:
    sys.exit("pre + post must be <= 375 ms (firmware capture limit of 6000 samples)")

BEGIN = re.compile(r"^RAW_BEGIN (.*)$")
SUMMARY = re.compile(r"^RAW_SUMMARY (.*)$")
BANNER = re.compile(r"^BUILD role=(\S+) git=(\S+) .*version=(\S+)(?: board=(\S+))?")
KV = re.compile(r"(\S+?)=(\S*)")


def kv(text):
    out = {}
    for k, v in KV.findall(text):
        try:
            out[k] = int(v)
        except ValueError:
            try:
                out[k] = float(v)
            except ValueError:
                out[k] = v
    return out


def parse_capture(lines):
    """RAW transcript -> (meta, rows); rows are (ms, pcm, amp or None) in sample order."""
    meta, fields, rows = {}, None, []
    for l in lines:
        m = BEGIN.match(l)
        if m:
            meta.update(kv(m.group(1)))
            continue
        m = SUMMARY.match(l)
        if m:
            meta["summary"] = kv(m.group(1))
            continue
        m = BANNER.match(l)
        if m:
            meta["firmware"] = {"role": m.group(1), "git": m.group(2), "version": m.group(3),
                                "board": m.group(4) or "piezo"}
            continue
        if l.startswith("ms,"):
            fields = l.split(",")
            continue
        if fields and re.match(r"^\d+,-?\d", l):
            p = l.split(",")
            if len(p) < len(fields):
                continue
            try:
                ms = int(p[0])
                pcm = int(float(p[fields.index("pcm")])) if "pcm" in fields else None
                amp = float(p[fields.index("amp")]) if "amp" in fields else None
            except (ValueError, IndexError):
                continue
            rows.append((ms, pcm, amp))
    meta["fields"] = fields
    return meta, rows


def write_wav(path, pcm, sr):
    """24-bit PCM mono. Values are the mic's 24-bit samples; clipped at +-2^23-1."""
    lim = (1 << 23) - 1
    body = bytearray()
    for v in pcm:
        v = max(-lim - 1, min(lim, v))
        body += struct.pack("<i", v)[:3]
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(body)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, sr, sr * 3, 3, 24))
        f.write(b"data" + struct.pack("<I", len(body)) + bytes(body))


def tone_check(meta, rows):
    """Single-bin DFT amplitude at f over the emit window, peak |pcm|, 5 ms envelope length."""
    sr = int(meta.get("sr", 16000))
    f = int(meta.get("f", 0) or 0)
    pcm = [r[1] for r in rows if r[1] is not None]
    if not pcm:
        return {}
    t0, t1 = meta.get("emit_start_ms"), meta.get("emit_done_ms")
    seg = [r[1] for r in rows if r[1] is not None and t0 is not None and t0 <= r[0] <= t1] or pcm
    mean = sum(seg) / len(seg)
    w = 2 * math.pi * f / sr
    re_ = sum((v - mean) * math.cos(w * n) for n, v in enumerate(seg))
    im_ = sum((v - mean) * math.sin(w * n) for n, v in enumerate(seg))
    amp = 2 * math.hypot(re_, im_) / len(seg)
    n5 = sr // 200
    env = [math.sqrt(sum(x * x for x in pcm[i:i + n5]) / n5) for i in range(0, len(pcm) - n5, n5)]
    pk = max(env) if env else 0
    return {"samples": len(pcm), "tone_amp": round(amp), "peak_abs": max(abs(v) for v in pcm),
            "env_len_ms": sum(1 for e in env if e > pk / 2) * 5}


def manifest_path(folder):
    return os.path.join(folder, "captures.json")


def load_manifest(folder):
    p = manifest_path(folder)
    if os.path.isfile(p):
        with open(p, encoding="utf-8") as f:
            return json.load(f)
    return {"purpose": "", "setup": {}, "captures": []}


def save_manifest(folder, data):
    with open(manifest_path(folder), "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=2)
        f.write("\n")


RAW_KEYS = ("sr", "f", "dur", "pre_ms", "post_ms", "mode", "emit_seen", "emit_start_ms",
            "emit_done_ms", "pre_samples", "post_samples")


def record(man, csv_path, meta, rows, label, rep, started):
    wav_path = csv_path[:-4] + ".wav"
    pcm = [r[1] for r in rows if r[1] is not None]
    if pcm:
        write_wav(wav_path, pcm, int(meta.get("sr", 16000)))
    chk = tone_check(meta, rows)
    entry = {"label": label, "rep": rep, "file": os.path.basename(csv_path),
             "wav": os.path.basename(wav_path) if pcm else None, "started": started,
             "firmware": meta.get("firmware"), "fields": meta.get("fields"),
             "raw": {k: meta[k] for k in RAW_KEYS if k in meta},
             "summary": meta.get("summary"), "check": chk}
    man["captures"] = [c for c in man["captures"] if c["file"] != entry["file"]] + [entry]
    print("%-28s %5d samples  tone_amp %7s  peak %7s  env %3s ms  emit_seen=%s" % (
        os.path.basename(csv_path), chk.get("samples", 0), chk.get("tone_amp", "-"),
        chk.get("peak_abs", "-"), chk.get("env_len_ms", "-"), meta.get("emit_seen", "?")))
    return entry


# --- convert existing csv files, no board -----------------------------------
if a.from_csv:
    if os.path.isfile(a.from_csv):
        files = [a.from_csv]
    else:
        files = sorted(glob.glob(os.path.join(a.from_csv, "*.csv")))
    folder = os.path.dirname(files[0]) if files else a.from_csv
    man = load_manifest(folder)
    n = 0
    for p in files:
        with open(p, encoding="utf-8", errors="replace") as f:
            lines = f.read().splitlines()
        if not any(BEGIN.match(l) for l in lines):
            continue
        meta, rows = parse_capture(lines)
        base = os.path.basename(p)[:-4]
        m = re.match(r"^(.*)_r(\d+)$", base)
        label, rep = (m.group(1), int(m.group(2))) if m else (base, 0)
        started = time.strftime("%Y-%m-%d", time.gmtime(os.path.getmtime(p)))
        record(man, p, meta, rows, label, rep, started)
        n += 1
    save_manifest(folder, man)
    print("%d captures -> %s" % (n, manifest_path(folder)))
    sys.exit(0)

# --- live captures ------------------------------------------------------------
if not a.port or not a.out:
    sys.exit("--port and --out are required for live captures (or use --from-csv)")
import serial  # noqa: E402

os.makedirs(a.out, exist_ok=True)
man = load_manifest(a.out)
if a.distance_cm is not None:
    man["setup"]["distance_cm"] = a.distance_cm
man["setup"]["orientation"] = a.orientation
if a.note:
    man["setup"]["notes"] = a.note
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


boot = pump(3.5)  # opening the port resets the board; keep the banner for the firmware hash
banner = next((l for l in boot if BANNER.match(l)), None)
if banner:
    print(banner)
for rep in range(a.reps):
    started = time.strftime("%Y-%m-%dT%H:%M:%S")
    cmd = "RAW trigger f=%d dur=%d pre=%d post=%d mode=%s\n" % (a.freq, a.dur, a.pre, a.post, a.mode)
    s.write(cmd.encode())
    lines = pump(20, "RAW_SUMMARY")
    if banner:
        lines = [banner] + lines
    csv_path = os.path.join(a.out, "%s_r%d.csv" % (a.label, rep))
    with open(csv_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    meta, rows = parse_capture(lines)
    if not rows:
        err = next((l for l in lines if l.startswith("RAW_ERR")), "no RAW_SUMMARY")
        print("%s: no data (%s)" % (csv_path, err))
        continue
    record(man, csv_path, meta, rows, a.label, rep, started)
    save_manifest(a.out, man)
    time.sleep(0.5)
save_manifest(a.out, man)
print("captures.json: %d entries in %s" % (len(man["captures"]), a.out))
