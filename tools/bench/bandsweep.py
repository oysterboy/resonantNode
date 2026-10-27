"""Frequency tuning sweep (step 11h, docs/research/frequency-tuning-sweep.md): the network band moved across 500-5000 Hz,
hits and noise acceptance per band position. Owner 2026-10-27: "how bad is detection / noise acceptance really".

A band position is its low edge; the band is --width wide (1000 Hz = 5 bins of 250 Hz, as shipped at 2950-3950).
The Node derives its bins and its hop train from the band (detection.band_low_hz / band_high_hz); the Analyzer and
the Emitter get the band, the hop order and the chirp frequency in each command.

host   No board. Replays noise through the real detection chain (native-replay --band LO-HI) at every position and
       counts what it accepts: the recorded noise corpus (RAW captures, the first --skip-ms hold a marker tone), the
       sections of a noise WAV (bench_audio.wav: music, two speakers, flute; resampled to 16 kHz once, cached) at
       several gains (a file has no mic level: the gains bracket it), and optionally synthetic traffic
       (synthtraffic.py at 16 kHz). Writes host.csv: position, source, gain, seconds, accepted, rejected, per hour.

         python tools/bench/bandsweep.py host --replay .pio/build/native-replay/program.exe --out <dir>
             [--positions 500:4000:250] [--wav C:/Users/admin/Downloads/bench_audio.wav] [--gains 1,4,16] [--traffic]

bench  The boards (one Analyzer with its Emitter, optionally Nodes). Per position: the Nodes' band set (unsaved,
       output.silent 1), the Analyzer's hop10 and chirp SEQ on that band (the chirp on the band's second bin, must be
       rejected), --raw-reps RAW hop captures (per-bin levels, hopbins.py), and with --noise-wav the Nodes listening
       for --noise-s while the lab PC plays the file (SoundPlayer, the default output device): their valid / rejected
       verdicts. At the end the Nodes get the shipped band back (unsaved; a reboot restores it too). Writes
       positions.csv (one row per SEQ step), noise.csv (one row per node and position), pos<low>_<step>.log,
       pos<low>_hop5_r<k>.csv, and nodes.log ("<t> <NAME> <line>", nodegroup_interim.py reads it).

         python tools/bench/bandsweep.py bench --analyzer COM3 [--nodes E1=COM8,E4=COM10,E5=COM11] --out <dir>
             [--positions 500:4000:250] [--noise-wav C:/Users/admin/Downloads/bench_audio.wav] [--hours 24]
             [--dry-run]

report A compact table from a bench run's CSVs (per position: hop accepted, strength, chirps accepted, noise per hour).

         python tools/bench/bandsweep.py report --out <dir>

Thresholds stay as shipped during a sweep: tuning follows the data, as its own pass.
"""
import argparse
import array
import csv
import datetime
import math
import operator
import os
import re
import subprocess
import sys
import time
import wave

GRID_HZ = 250  # RuntimeDefaults.h kBandBinSpacingHz (16 kHz / 64 samples)
SHIPPED_BAND = (2950, 3950)
RATE = 16000


def parse_positions(text):
    """'1000:6500:500' (start:stop:step, stop included) or '1000,1500,3000'."""
    if ":" in text:
        start, stop, step = (int(x) for x in text.split(":"))
        return list(range(start, stop + 1, step))
    return [int(x) for x in text.split(",") if x.strip()]


def band_of(low, width):
    return low, low + width


def bins_of(low, high):
    return list(range(low, high + 1, GRID_HZ))


# ---------------------------------------------------------------------------------------------------------- host

def read_wav_mono(path):
    w = wave.open(path, "rb")
    n, ch, sw, rate = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
    raw = w.readframes(n)
    w.close()
    if sw != 2:
        sys.exit("bandsweep: %s: 16-bit WAV only" % path)
    a = array.array("h")
    a.frombytes(raw)
    if ch > 1:
        a = array.array("h", (sum(a[i:i + ch]) // ch for i in range(0, len(a), ch)))
    return a, rate


def resample(x, rate_in, rate_out=RATE, half_taps=16):
    """Windowed-sinc resampling (Hann), low-passed below 0.9 x the output Nyquist so music above 8 kHz does not fold
    into the band. Plain Python, a phase table for rational ratios; minutes for a 7 min file, so the result is
    cached by the caller."""
    if rate_in == rate_out:
        return array.array("h", x)
    g = math.gcd(rate_in, rate_out)
    up, down = rate_out // g, rate_in // g  # output n sits at input position n * down / up
    cutoff = 0.9 * min(rate_in, rate_out) / 2.0 / rate_in  # cycles per input sample
    taps = 2 * half_taps
    table = []
    for p in range(up):
        frac = p / up
        row = []
        for k in range(taps):
            t = (k - half_taps + 1) - frac
            s = 2 * cutoff * (math.sin(2 * math.pi * cutoff * t) / (2 * math.pi * cutoff * t) if t else 1.0)
            w = 0.5 + 0.5 * math.cos(math.pi * t / (half_taps + 1))
            row.append(s * w)
        table.append(row)
    n_out = len(x) * up // down
    out = array.array("h", bytes(2 * n_out))
    xs = list(x)
    pad = [0] * taps
    xs = pad + xs + pad
    for n in range(n_out):
        pos = n * down
        i, p = divmod(pos, up)
        start = i - half_taps + 1 + taps
        v = sum(map(operator.mul, table[p], xs[start:start + taps]))
        out[n] = max(-32768, min(32767, int(round(v))))
    return out


def write_wav24(path, samples, gain):
    """16 kHz mono 24-bit WAV (native-replay reads 24-bit), samples x gain: room above 16 bits for the gains."""
    buf = bytearray()
    for s in samples:
        v = max(-8388608, min(8388607, int(round(s * gain))))
        buf += (v & 0xFFFFFF).to_bytes(3, "little")
    w = wave.open(path, "wb")
    w.setnchannels(1)
    w.setsampwidth(3)
    w.setframerate(RATE)
    w.writeframes(bytes(buf))
    w.close()


def sections_for(wav_path):
    """bench_audio.sections.txt next to this tool when the file is bench_audio.wav; else the whole file."""
    sec = os.path.join(os.path.dirname(os.path.abspath(__file__)), "bench_audio.sections.txt")
    if os.path.basename(wav_path) == "bench_audio.wav" and os.path.exists(sec):
        out = []
        for line in open(sec, encoding="utf-8"):
            if line.startswith("#") or not line.strip():
                continue
            start, end, title = line.split()[:3]
            if not title.startswith("silence"):
                out.append((title, float(start), float(end)))
        return out
    return None


def build_noise_sources(a):
    """[(source, gain, wav path or dir, seconds, skip_ms)]: the corpus dirs, the WAV sections, synthetic traffic."""
    os.makedirs(a.cache, exist_ok=True)
    sources = []
    for d in a.corpus:  # seconds from the replay's own samples= (None until then)
        sources.append(("corpus:" + os.path.basename(d.rstrip("/\\")), 1.0, d, None, a.skip_ms))
    gains = [float(g) for g in a.gains.split(",")]
    if a.wav:
        base = os.path.splitext(os.path.basename(a.wav))[0]
        cached16 = os.path.join(a.cache, base + "_16k.raw")
        if os.path.exists(cached16):
            x16 = array.array("h")
            x16.frombytes(open(cached16, "rb").read())
        else:
            x, rate = read_wav_mono(a.wav)
            print("resampling %s (%.0f s at %d Hz) to 16 kHz, once..." % (a.wav, len(x) / rate, rate), flush=True)
            x16 = resample(x, rate)
            open(cached16, "wb").write(x16.tobytes())
        parts = sections_for(a.wav) or [(base, 0.0, len(x16) / RATE)]
        for title, start, end in parts:
            seg = x16[int(start * RATE):int(end * RATE)]
            for g in gains:
                p = os.path.join(a.cache, "%s_g%g.wav" % (title, g))
                if not os.path.exists(p):
                    write_wav24(p, seg, g)
                sources.append((title, g, p, len(seg) / RATE, 0))
    if a.traffic:
        tool = os.path.join(os.path.dirname(os.path.abspath(__file__)), "synthtraffic.py")
        t16 = os.path.join(a.cache, "traffic_synth_16k.wav")
        if not os.path.exists(t16):
            subprocess.run([sys.executable, tool, "--out", t16, "--seconds", "150", "--rate", str(RATE)], check=True)
        x, _ = read_wav_mono(t16)
        for g in gains:
            p = os.path.join(a.cache, "traffic_synth_g%g.wav" % g)
            if not os.path.exists(p):
                write_wav24(p, x, g)
            sources.append(("traffic_synth", g, p, len(x) / RATE, 0))
    return sources


def replay_counts(prog, target, band, skip_ms):
    cmd = [prog, "--band", "%d-%d" % band]
    if skip_ms:
        cmd += ["--skip-ms", str(skip_ms)]
    cmd += ["--dir", target] if os.path.isdir(target) else [target]
    out = subprocess.run(cmd, capture_output=True, text=True).stdout
    m = re.search(r"summary captures=(\d+) accepted=(\d+) rejected=(\d+)", out)
    if not m:
        sys.exit("bandsweep: no replay summary for %s: %s" % (target, out[-500:]))
    captures, acc, rej = int(m.group(1)), int(m.group(2)), int(m.group(3))
    samples = sum(int(x) for x in re.findall(r"samples=\s*(\d+)", out))
    seconds = max(0, samples - captures * skip_ms * RATE // 1000) / RATE  # what the detector heard after the skip
    return acc, rej, seconds


def run_host(a):
    os.makedirs(a.out, exist_ok=True)
    sources = build_noise_sources(a)
    path = os.path.join(a.out, "host.csv")
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["band_low_hz", "band_high_hz", "source", "gain", "seconds", "accepted", "rejected",
                    "accepted_per_hour"])
        for low in parse_positions(a.positions):
            band = band_of(low, a.width)
            line = []
            for source, gain, target, seconds, skip in sources:
                acc, rej, heard = replay_counts(a.replay, target, band, skip)
                seconds = heard if seconds is None else seconds
                per_hour = acc * 3600.0 / seconds if seconds > 0 else 0.0
                w.writerow([band[0], band[1], source, gain, "%.1f" % seconds, acc, rej, "%.1f" % per_hour])
                if acc:
                    line.append("%s x%g: %d" % (source, gain, acc))
            f.flush()
            print("%d-%d Hz: %s" % (band[0], band[1], "; ".join(line) if line else "no noise accepted"), flush=True)
    print("wrote", path)


# --------------------------------------------------------------------------------------------------------- bench

def bench_steps(low, high):
    """The Analyzer's SEQ steps and the RAW capture line for one band position (soakcycle.py's, on this band)."""
    hop = bins_of(low, high)
    center = hop[len(hop) // 2]
    band = "%d-%d" % (low, high)
    hop_line = "freq=%d band=%s dur=25 pulses=5 gap=10 hop=%s" % (center, band, ",".join(str(h) for h in hop[:5]))
    steps = [
        ("hop10", "SEQ start profile=TonalPulseScalar tries=10 %s mode=detail when=all verbose=1" % hop_line),
        ("chirp150", "SEQ start profile=TonalPulseScalar tries=10 freq=%d dur=150 band=%s mode=detail when=all verbose=1"
         % (hop[1], band)),
    ]
    raw = ("RAW trigger f=%d dur=25 pulses=5 gap=10 hop=%s pre=50 post=250 mode=both band=%s"
           % (center, ",".join(str(h) for h in hop[:5]), band))
    return steps, raw


def summarize_seq(lines):
    row = {}
    for l in lines:
        if l.startswith("SEQ_SUMMARY"):
            f = dict(re.findall(r"([\w.]+)=(\S+)", l))
            for k in ("expected_trials", "rejected_trials", "miss_trials", "unexpected_trials", "avg_strength"):
                row[k] = f.get(k, "")
    reasons = {}
    for l in lines:
        r = None
        if l.startswith("SEQ_TRIAL"):
            r = re.search(r"reject_reason=(\w+)", l)
            r = r.group(1) if r and r.group(1) not in ("none", "result_in_expected_window") else None
        elif l.startswith("SEQ_EXPLAIN"):
            r = re.search(r"failed_label=(\w+)", l)
            r = "failed_" + r.group(1) if r and r.group(1) != "none" else None
        if r:
            reasons[r] = reasons.get(r, 0) + 1
    row["reasons"] = ";".join("%s:%d" % kv for kv in sorted(reasons.items()))
    return row


class Bench:
    """The ports, held for the whole sweep with DTR/RTS low (no reset); node lines to nodes.log as soakcycle does."""

    def __init__(self, a):
        import serial  # only the bench mode needs pyserial
        self.serial = serial
        self.t0 = time.time()
        self.nodes = {}
        for item in (a.nodes or "").split(","):
            if item:
                name, port = item.split("=")
                self.nodes[name] = self.open(port)
        self.analyzer = self.open(a.analyzer)
        self.partial = {n: b"" for n in list(self.nodes) + ["AN"]}
        self.sink = None
        self.counting = None  # {node: [valid, rejected]} while a noise window runs
        self.log = open(os.path.join(a.out, "nodes.log"), "w", encoding="utf-8")
        self.log.write("# bandsweep nodes=%s analyzer=%s started_unix=%.3f started=%s\n" % (
            a.nodes, a.analyzer, self.t0, datetime.datetime.now().isoformat(timespec="seconds")))

    def open(self, port):
        s = self.serial.Serial()
        s.port, s.baudrate, s.timeout = port, 115200, 0
        s.dtr = False
        s.rts = False
        s.open()
        return s

    def lines_from(self, name, s):
        self.partial[name] += s.read(s.in_waiting or 0)
        out = []
        while b"\n" in self.partial[name]:
            line, self.partial[name] = self.partial[name].split(b"\n", 1)
            line = line.decode("ascii", errors="replace").strip()
            if line:
                out.append(line)
        return out

    def pump(self, seconds=0.0):
        end = time.time() + seconds
        while True:
            for n, s in self.nodes.items():
                for line in self.lines_from(n, s):
                    self.log.write("%.2f %s %s\n" % (time.time() - self.t0, n, line))
                    if self.counting is not None and line.startswith("RB verdict="):
                        self.counting[n][0 if "valid=1" in line else 1] += 1
            for line in self.lines_from("AN", self.analyzer):
                if self.sink is not None:
                    self.sink.append(line)
            if time.time() >= end:
                break
            time.sleep(0.005)

    def to_nodes(self, cmd):
        for n, s in self.nodes.items():
            s.write((cmd + "\n").encode())
            self.log.write("%.2f %s >> %s\n" % (time.time() - self.t0, n, cmd))
            self.pump(0.2)

    def analyzer_run(self, cmd, done_on, timeout_s):
        self.sink = []
        self.analyzer.write((cmd + "\n").encode())
        start = time.time()
        done_at = None
        while time.time() - start < timeout_s:
            self.pump(0.5)
            if done_at is None and any(l.startswith(done_on) for l in self.sink):
                done_at = time.time()
            if done_at is not None and time.time() - done_at > 2:
                break
        got, self.sink = self.sink, None
        return [">> " + cmd] + got


def play_noise(wav):
    """Plays the WAV on the lab PC's default output device (PowerShell SoundPlayer); the caller kills it."""
    ps = "(New-Object System.Media.SoundPlayer '%s').PlaySync()" % wav.replace("'", "''")
    return subprocess.Popen(["powershell", "-NoProfile", "-Command", ps])


def wav_seconds(path):
    w = wave.open(path, "rb")
    seconds = w.getnframes() / float(w.getframerate())
    w.close()
    return seconds


def round_order(positions, reference, rnd, seed):
    """One round: the reference band first (the shipped band, the day's yardstick), then the rest shuffled per round
    so time of day and room drift do not line up with frequency."""
    import random
    rest = [p for p in positions if p != reference]
    random.Random(seed * 1000 + rnd).shuffle(rest)
    return ([reference] if reference else []) + rest


def run_position(bench, a, low, rnd, pos_csv, noise_csv, pos_file, noise_file):
    low, high = band_of(low, a.width)
    stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    tag = "pos%d_r%02d" % (low, rnd)
    bench.log.write("%.2f -- round %d position %d-%d start\n" % (time.time() - bench.t0, rnd, low, high))
    bench.to_nodes("PARAM SET detection.band_low_hz %d" % low)
    bench.to_nodes("PARAM SET detection.band_high_hz %d" % high)
    bench.pump(a.settle_s)
    steps, raw = bench_steps(low, high)
    summary = []
    for name, cmd in steps:
        lines = bench.analyzer_run(cmd, "SEQ_SUMMARY", 150)
        open(os.path.join(a.out, "%s_%s.log" % (tag, name)), "w", encoding="utf-8").write("\n".join(lines) + "\n")
        r = summarize_seq(lines)
        pos_csv.writerow([rnd, low, high, stamp, name, r.get("expected_trials", ""), r.get("rejected_trials", ""),
                          r.get("miss_trials", ""), r.get("unexpected_trials", ""), r.get("avg_strength", ""),
                          r.get("reasons", "")])
        pos_file.flush()
        summary.append("%s %s/10" % (name, r.get("expected_trials", "?")))
    for k in range(a.raw_reps):
        lines = bench.analyzer_run(raw, "RAW_SUMMARY", 30)
        open(os.path.join(a.out, "%s_hop5_%d.csv" % (tag, k)), "w", encoding="utf-8").write(
            "\n".join(l for l in lines if not l.startswith(">> ")) + "\n")
    if a.noise_wav and (bench.nodes or a.under_noise):
        seconds = a.noise_s if a.noise_s > 0 else int(math.ceil(wav_seconds(a.noise_wav)))
        bench.counting = {n: [0, 0] for n in bench.nodes}
        proc = play_noise(a.noise_wav)
        started = time.time()
        bench.log.write("%.2f -- noise %s %d s\n" % (time.time() - bench.t0, a.noise_wav, seconds))
        if a.under_noise:
            # Hits under noise (owner 2026-10-27): hop10 back to back while the file plays, each run labelled by the
            # file section it started in. Does a train get through while the detector is busy with music or speech?
            sections = sections_for(a.noise_wav) or []
            hop_name, hop_cmd = steps[0]
            k = 0
            while time.time() - started < seconds - 20:
                offset = time.time() - started
                section = next((t for t, x, y in sections if x <= offset < y), "gap")
                lines = bench.analyzer_run(hop_cmd, "SEQ_SUMMARY", 150)
                k += 1
                open(os.path.join(a.out, "%s_%s_noise%d.log" % (tag, hop_name, k)), "w", encoding="utf-8").write(
                    "\n".join(lines) + "\n")
                r = summarize_seq(lines)
                pos_csv.writerow([rnd, low, high, stamp, "%s_noise@%ds_%s" % (hop_name, offset, section),
                                  r.get("expected_trials", ""), r.get("rejected_trials", ""), r.get("miss_trials", ""),
                                  r.get("unexpected_trials", ""), r.get("avg_strength", ""), r.get("reasons", "")])
                pos_file.flush()
                summary.append("%s@%s %s/10" % (hop_name, section[:8], r.get("expected_trials", "?")))
            bench.pump(max(0.0, seconds - (time.time() - started)))
        else:
            bench.pump(seconds)
        proc.kill()
        counts, bench.counting = bench.counting, None
        for n, (valid, rejected) in counts.items():
            noise_csv.writerow([rnd, low, high, stamp, n, seconds, valid, rejected, "%.1f" % (valid * 3600.0 / seconds)])
        noise_file.flush()
        summary.append("noise valid " + ",".join("%s:%d" % (n, c[0]) for n, c in counts.items()))
    bench.log.write("%.2f -- round %d position %d-%d end\n" % (time.time() - bench.t0, rnd, low, high))
    bench.log.flush()
    print("r%d %d-%d %s" % (rnd, low, high, "; ".join(summary)), flush=True)  # one line per position: cheap to read


def run_bench(a):
    os.makedirs(a.out, exist_ok=True)
    positions = parse_positions(a.positions)
    if a.dry_run:
        for low in round_order(positions, a.reference, 1, a.seed):
            low, high = band_of(low, a.width)
            steps, raw = bench_steps(low, high)
            print("== %d-%d Hz" % (low, high))
            if a.nodes:
                print("  nodes >> PARAM SET detection.band_low_hz %d / band_high_hz %d" % (low, high))
            for name, cmd in steps:
                print("  analyzer [%s] >> %s" % (name, cmd))
            print("  analyzer [raw x%d] >> %s" % (a.raw_reps, raw))
            if a.noise_wav and a.under_noise:
                print("  under noise: hop10 back to back while %s plays" % a.noise_wav)
            if a.noise_wav and a.nodes:
                print("  noise: play %s for %s s, count the Nodes' verdicts"
                      % (a.noise_wav, a.noise_s if a.noise_s > 0 else "the whole file"))
        return
    bench = Bench(a)
    pos_path = os.path.join(a.out, "positions.csv")
    noise_path = os.path.join(a.out, "noise.csv")
    pos_file = open(pos_path, "a", newline="", encoding="utf-8")
    noise_file = open(noise_path, "a", newline="", encoding="utf-8")
    pos_csv, noise_csv = csv.writer(pos_file), csv.writer(noise_file)
    if pos_file.tell() == 0:
        pos_csv.writerow(["round", "band_low_hz", "band_high_hz", "local_time", "step", "expected", "rejected", "miss",
                          "unexpected", "avg_strength", "reasons"])
    if noise_file.tell() == 0:
        noise_csv.writerow(["round", "band_low_hz", "band_high_hz", "local_time", "node", "seconds", "valid",
                            "rejected", "valid_per_hour"])
    deadline = bench.t0 + a.hours * 3600.0 if a.hours > 0 else None
    try:
        bench.to_nodes("PARAM SET output.silent 1")
        bench.to_nodes("RB log full")
        rnd = 0
        while True:
            rnd += 1
            for low in round_order(positions, a.reference, rnd, a.seed):
                if deadline is not None and time.time() >= deadline:
                    break
                run_position(bench, a, low, rnd, pos_csv, noise_csv, pos_file, noise_file)
            if deadline is None or time.time() >= deadline:
                break
    finally:
        bench.to_nodes("PARAM SET detection.band_low_hz %d" % SHIPPED_BAND[0])
        bench.to_nodes("PARAM SET detection.band_high_hz %d" % SHIPPED_BAND[1])
        bench.pump(1)
        bench.log.close()
        pos_file.close()
        noise_file.close()
    print("done: %d round(s), %.2f h; %s" % (rnd, (time.time() - bench.t0) / 3600.0, pos_path), flush=True)


def run_report(a):
    """A compact table from positions.csv / noise.csv: per position, hop accepted / tried, chirps accepted (should be
    0), the hop strength range, noise valid verdicts per hour per node (mean over rounds). Cheap to read."""
    hop, chirp, strength, noise, under = {}, {}, {}, {}, {}
    for r in csv.DictReader(open(os.path.join(a.out, "positions.csv"), encoding="utf-8")):
        low = int(r["band_low_hz"])
        exp = int(r["expected"] or 0)
        tried = exp + int(r["rejected"] or 0) + int(r["miss"] or 0)
        if r["step"].startswith("hop10_noise"):
            u = under.setdefault(low, [0, 0])
            u[0] += exp
            u[1] += tried
        elif r["step"] == "hop10":
            h = hop.setdefault(low, [0, 0])
            h[0] += exp
            h[1] += tried
            if r["avg_strength"]:
                strength.setdefault(low, []).append(float(r["avg_strength"]))
        else:
            c = chirp.setdefault(low, [0, 0])
            c[0] += exp
            c[1] += tried
    path = os.path.join(a.out, "noise.csv")
    if os.path.exists(path):
        for r in csv.DictReader(open(path, encoding="utf-8")):
            n = noise.setdefault(int(r["band_low_hz"]), {}).setdefault(r["node"], [0, 0.0])
            n[0] += int(r["valid"])
            n[1] += float(r["seconds"])
    print("band       hop acc   strength        chirp acc   hop under noise   noise valid/h per node")
    for low in sorted(set(hop) | set(chirp) | set(noise)):
        h, c, s = hop.get(low, [0, 0]), chirp.get(low, [0, 0]), strength.get(low, [])
        u = "%d/%d" % tuple(under[low]) if low in under else "-"
        nz = " ".join("%s %.1f" % (n, v[0] * 3600.0 / v[1]) for n, v in sorted(noise.get(low, {}).items()) if v[1])
        print("%4d-%4d  %3d/%-3d  %6s-%-6s  %3d/%-3d     %-17s %s" % (
            low, low + a.width, h[0], h[1], "%.0f" % min(s) if s else "-", "%.0f" % max(s) if s else "-",
            c[0], c[1], u, nz))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = ap.add_subparsers(dest="mode", required=True)
    for name in ("host", "bench", "report"):
        p = sub.add_parser(name)
        p.add_argument("--out", required=True)
        p.add_argument("--positions", default="500:4000:250",
                       help="band low edges: start:stop:step or a,b,c (default: the tones 500-5000 Hz, owner 2026-10-27)")
        p.add_argument("--width", type=int, default=1000, help="band width in Hz (1000 = 5 bins)")
        if name == "host":
            p.add_argument("--replay", required=True, help="the native-replay program")
            p.add_argument("--corpus", nargs="*", default=["bench/sessions/2026-10-16-corpus-noise"])
            p.add_argument("--skip-ms", type=int, default=100, help="the corpus captures' marker tone")
            p.add_argument("--wav", help="a noise WAV (bench_audio.wav: split by its sections file)")
            p.add_argument("--gains", default="1,4,16", help="gains for the WAV sources (24-bit replay input)")
            p.add_argument("--traffic", action="store_true", help="add synthtraffic.py at 16 kHz")
            p.add_argument("--cache", default="logs/bandsweep_cache", help="resampled audio (gitignored)")
        elif name == "report":
            pass
        else:
            p.add_argument("--analyzer", required=False)
            p.add_argument("--nodes", default="", help="NAME=PORT,... (silent listeners; their band follows)")
            p.add_argument("--raw-reps", type=int, default=1)
            p.add_argument("--hours", type=float, default=0.0, help="repeat rounds until this many hours (0: one round)")
            p.add_argument("--reference", type=int, default=SHIPPED_BAND[0], help="band low edge each round starts with")
            p.add_argument("--seed", type=int, default=1, help="the shuffle of each round's order")
            p.add_argument("--settle-s", type=float, default=5.0)
            p.add_argument("--noise-wav")
            p.add_argument("--noise-s", type=int, default=0, help="0: the whole file")
            p.add_argument("--under-noise", action="store_true",
                           help="during the noise, run hop10 back to back (hits under noise; needs --noise-wav)")
            p.add_argument("--dry-run", action="store_true")
    a = ap.parse_args(argv)
    for low in parse_positions(a.positions):
        if low % 50 or a.width % GRID_HZ or low + a.width >= RATE // 2:
            sys.exit("bandsweep: position %d with width %d: low edge a multiple of 50 (BinTrack's code), width a "
                     "multiple of %d, top below 8 kHz" % (low, a.width, GRID_HZ))
    if a.mode == "host":
        run_host(a)
    elif a.mode == "report":
        run_report(a)
    else:
        if not a.dry_run and not a.analyzer:
            sys.exit("bandsweep bench: --analyzer is required (or --dry-run)")
        run_bench(a)


if __name__ == "__main__":
    main()
