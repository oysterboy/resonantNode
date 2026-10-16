"""Assemble one reproducible test audio file from WAV clips, plus its section list.

Layout: lead silence, then one section per clip (default 100 s each, looped or
cut from --start), short silences between, tail silence. Writes OUT.wav (mono,
16 bit) and OUT.sections.txt for tools/bench/inspectstats.py --sections.

Inputs are WAV files (convert mp3 etc. first, e.g. with Audacity or VLC). Each
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
ap.add_argument("clips", nargs="+", help="TITLE=path[@start_s]")
args = ap.parse_args()


def read_mono(path, rate):
    with wave.open(path, "rb") as w:
        ch, width, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width != 2:
        sys.exit("%s: only 16 bit WAV supported (got %d bit)" % (path, width * 8))
    a = array.array("h")
    a.frombytes(raw)
    if sys.byteorder == "big":
        a.byteswap()
    mono = [sum(a[i:i + ch]) / ch for i in range(0, len(a), ch)] if ch > 1 else [float(x) for x in a]
    if sr != rate:
        step = sr / rate
        out = []
        for i in range(int((len(mono) - 1) / step)):
            p = i * step
            j = int(p)
            f = p - j
            out.append(mono[j] * (1 - f) + mono[j + 1] * f)
        mono = out
    return mono


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
    pcm = read_mono(path, rate)
    sec = section(pcm, float(start) if start else 0.0, int(args.section_s * rate), rate, rms_target)
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
