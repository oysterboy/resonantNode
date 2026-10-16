"""Assemble one reproducible test audio file from WAV clips, plus its section list.

Layout: lead silence, then one section per clip (default 100 s each, looped or
cut from --start), short silences between, tail silence. Writes OUT.wav (mono,
16 bit) and OUT.sections.txt for tools/bench/inspectstats.py --sections.

Inputs are WAV files (16/24/32 bit) or mp3 (needs the miniaudio module in a separate venv). Each
input is  TITLE=path[@start_s]  e.g.  music_A=dnb.wav@30  speech_A=wdr.wav

Example:
  python tools/bench/buildaudio.py --out bench_audio.wav \
      music_A=dnb.wav@30 music_B=techno.wav speech_A=m.wav speech_B=f.wav tonal_hard_test=flute.wav

Sections are scaled to the same RMS (--rms-dbfs, default -20) so a run measures
the detector, not the clip loudness; --no-normalize keeps the clip level. Play
the file at a fixed player level, same speaker, 50 cm, 90 deg to the mic.
Pure standard library (no numpy), so it runs with the PlatformIO python.
"""
import argparse
import array
import math
import sys
import wave

ap = argparse.ArgumentParser()
ap.add_argument("--out", required=True)
ap.add_argument("--rate", type=int, default=44100)
ap.add_argument("--section-s", type=float, default=100.0)
ap.add_argument("--lead-s", type=float, default=10.0)
ap.add_argument("--gap-s", type=float, default=5.0)
ap.add_argument("--tail-s", type=float, default=10.0)
ap.add_argument("--rms-dbfs", type=float, default=-20.0)
ap.add_argument("--no-normalize", action="store_true")
ap.add_argument("--peak-safe", action="store_true",
                help="limit the gain of a section so its peak stays below 31000 (the section is then quieter than --rms-dbfs, reported)")
ap.add_argument("--hp", action="append", default=[], metavar="TITLE=HZ",
                help="2nd-order Butterworth high-pass for one clip (attenuate bass), repeatable")
ap.add_argument("--bass-db", action="append", default=[], metavar="TITLE=DB",
                help="needs --hp TITLE=HZ: the part below that corner is added back at DB relative to its natural level; the level above the corner (detector band) stays as normalised")
ap.add_argument("clips", nargs="+", help="TITLE=path[@start_s]")
args = ap.parse_args()


def read_mono(path, rate, start_s, need_s):
    """Mono float samples at `rate`: only the part from start_s, about need_s long (looped later)."""
    if path.lower().endswith(".mp3"):
        try:
            import miniaudio
        except ImportError:
            sys.exit("mp3 needs the miniaudio module (pip install miniaudio in a separate venv)")
        dec = miniaudio.decode_file(path, output_format=miniaudio.SampleFormat.SIGNED16, nchannels=1, sample_rate=rate)
        a = dec.samples
        lo = int(start_s * rate)
        return [float(x) for x in a[lo:lo + int(need_s * rate) + 2]]
    with wave.open(path, "rb") as w:
        ch, width, sr = w.getnchannels(), w.getsampwidth(), w.getframerate()
        w.setpos(min(int(start_s * sr), max(w.getnframes() - 1, 0)))
        raw = w.readframes(int(need_s * sr) + 2)
    n = len(raw) // (width * ch)
    if width == 2:
        a = array.array("h")
        a.frombytes(raw[:n * ch * 2])
        if sys.byteorder == "big":
            a.byteswap()
        vals = a
    elif width == 3:
        vals = [int.from_bytes(raw[k:k + 3], "little", signed=True) / 256.0 for k in range(0, n * ch * 3, 3)]
    elif width == 4:
        a = array.array("i")
        a.frombytes(raw[:n * ch * 4])
        vals = [x / 65536.0 for x in a]
    else:
        sys.exit("%s: unsupported WAV width %d bit" % (path, width * 8))
    mono = [sum(vals[i:i + ch]) / ch for i in range(0, n * ch, ch)] if ch > 1 else [float(x) for x in vals]
    if sr != rate:
        step = sr / rate
        mono = [mono[int(i * step)] * (1 - (i * step - int(i * step))) + mono[min(int(i * step) + 1, len(mono) - 1)] * (i * step - int(i * step))
                for i in range(int((len(mono) - 1) / step))]
    return mono


def highpass(x, fc, rate):
    """2nd-order Butterworth high-pass (biquad), pure python."""
    w0 = 2 * math.pi * fc / rate
    alpha = math.sin(w0) / math.sqrt(2.0)
    cw = math.cos(w0)
    b0, b1, b2 = (1 + cw) / 2, -(1 + cw), (1 + cw) / 2
    a0, a1, a2 = 1 + alpha, -2 * cw, 1 - alpha
    b0, b1, b2, a1, a2 = b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0
    x1 = x2 = y1 = y2 = 0.0
    out = []
    for v in x:
        y = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, v, y1, y
        out.append(y)
    return out


def section(samples, start_s, length, rate, rms_target):
    start = int(start_s * rate)
    seg = samples[start:] if start < len(samples) else samples
    if not seg:
        sys.exit("clip is empty after --start")
    out = []
    while len(out) < length:
        out.extend(seg[:length - len(out)])
    if rms_target is not None:
        rms = math.sqrt(sum(x * x for x in out) / len(out))
        if rms > 0:
            g = rms_target / rms
            pk = max(abs(x) for x in out)
            if args.peak_safe and pk * g > 31000.0:
                g = 31000.0 / pk
                print("peak-safe: section limited to %.1f dBFS RMS" % (20 * math.log10(rms * g / 32768.0)))
            out = [x * g for x in out]
    fade = int(0.02 * rate)
    for i in range(min(fade, len(out))):
        k = i / fade
        out[i] *= k
        out[-1 - i] *= k
    return out


rate = args.rate
rms_target = None if args.no_normalize else 32768.0 * 10 ** (args.rms_dbfs / 20.0)
silence = lambda s: [0.0] * int(s * rate)
audio = silence(args.lead_s)
sections = [(0.0, args.lead_s, "silence_lead")]
for idx, spec in enumerate(args.clips):
    title, _, rest = spec.partition("=")
    if not rest:
        sys.exit("clip must be TITLE=path[@start_s]: %s" % spec)
    path, _, start = rest.partition("@")
    start_s = float(start) if start else 0.0
    pcm = read_mono(path, rate, start_s, args.section_s)
    hp = dict(h.split('=', 1) for h in args.hp).get(title)
    bass = dict(h.split('=', 1) for h in args.bass_db).get(title)
    if bass is not None:
        if not hp:
            sys.exit("--bass-db %s needs --hp %s=HZ" % (title, title))
        hi = highpass(pcm, float(hp), rate)
        lo = [x - y for x, y in zip(pcm, hi)]
        n = int(args.section_s * rate)
        hi_s = section(hi, 0.0, n, rate, None)
        lo_s = section(lo, 0.0, n, rate, None)
        rms_hi = math.sqrt(sum(x * x for x in hi_s) / n)
        g = rms_target / rms_hi if rms_target and rms_hi else 1.0
        k = 10 ** (float(bass) / 20.0)
        while k > 0.01:
            pk = max(abs(a_ + k * b_) for a_, b_ in zip(hi_s, lo_s)) * g
            if pk <= 31000.0:
                break
            k *= 0.89
        print("bass %s: %+.1f dB requested, %+.1f dB used (peak %.0f)" % (title, float(bass), 20 * math.log10(k), pk))
        sec = [(a_ + k * b_) * g for a_, b_ in zip(hi_s, lo_s)]
    else:
        if hp:
            pcm = highpass(pcm, float(hp), rate)
        sec = section(pcm, 0.0, int(args.section_s * rate), rate, rms_target)
    t0 = len(audio) / rate
    audio.extend(sec)
    sections.append((t0, t0 + args.section_s, title))
    if idx < len(args.clips) - 1:
        audio.extend(silence(args.gap_s))
t0 = len(audio) / rate
audio.extend(silence(args.tail_s))
sections.append((t0, t0 + args.tail_s, "silence_tail"))

peak = max((abs(x) for x in audio), default=0.0)
if peak > 32767:
    sys.exit("clipping after normalising (peak %.0f); lower --rms-dbfs" % peak)
pcm16 = array.array("h", (int(round(x)) for x in audio))
if sys.byteorder == "big":
    pcm16.byteswap()
with wave.open(args.out, "wb") as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(rate)
    w.writeframes(pcm16.tobytes())

sec_path = args.out.rsplit(".", 1)[0] + ".sections.txt"
with open(sec_path, "w", encoding="utf-8") as f:
    f.write("# START END TITLE (seconds from file start), rate %d Hz, section %.0f s, rms %s\n" % (
        rate, args.section_s, "unchanged" if rms_target is None else "%.1f dBFS" % args.rms_dbfs))
    for a, b, t in sections:
        f.write("%.1f %.1f %s\n" % (a, b, t))
print("wrote %s (%.1f s) and %s" % (args.out, len(audio) / rate, sec_path))
