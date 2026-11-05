"""Whispernet's band checks against recordings: the labelled WAVs of a labelrec.py session are played through the lab
PC's speaker to a whisper-build board, once per setting (default / level_band / timbre_check / both), and the board's
own counters (WHISPER STATUS: phrases, whispers) give per label how many phrases it judged and how many passed.
Runs through a radio gateway board (E0) or a Node on USB; the other boards should be silent.

CAVEAT (2026-11-05, bench/sessions/2026-11-05-whisperband): a small speaker and the room colour the spectrum, so
a recorded whisper played back read flatness 0.01-0.07 against 0.15-0.49 live and failed the timbre check at every
setting; the levels are not the board's either. Use it for the level and the phrase count, not for the timbre; the
band features are read on the recordings themselves (research/whispernet.md, native-echoreplay --band-level / --low-max).

  python tools/bench/whisperband.py COM12 E5 DIR [DIR ...] [--out FILE] [--settle 40] [--gain G]

The board: esp32dev-whisper, whisper.gain 0 here (it judges, it does not answer aloud), whisper.aec 0. The files are
scaled by one gain for all labels (so their levels stay in proportion); without --gain the script plays a whisper
file at 2 / 4 / 8 / 16 x and takes the gain whose phrase level (level_p90 of the WHISPER lines) comes nearest 450,
where the owner's desk whispers read 300-860 (Strength16 units). Not saved on the board, the next reflash resets it.
"""
import argparse
import array
import os
import re
import subprocess
import sys
import tempfile
import time
import wave

import serial

ap = argparse.ArgumentParser()
ap.add_argument("port")
ap.add_argument("board")
ap.add_argument("dirs", nargs="+")
ap.add_argument("--out", default="")
ap.add_argument("--settle", type=float, default=40.0)
ap.add_argument("--gain", type=float, default=0.0)
ap.add_argument("--tail", type=float, default=6.0)
a = ap.parse_args()

s = serial.Serial()
s.port, s.baudrate, s.timeout = a.port, 115200, 0.05
s.dtr = False
s.rts = False
s.open()
B = a.board
lines_all = []


def read_for(seconds):
    end, text = time.time() + seconds, ""
    while time.time() < end:
        text += s.read(65536).decode(errors="replace")
    lines_all.append(text)
    return text


def send(line, wait=1.6):
    s.reset_input_buffer()
    s.write((line + "\n").encode())
    return read_for(wait)


def setp(path, value):
    return "status=ok" in send("NET %s PARAM SET %s %s" % (B, path, value), 1.8)


def status():
    for _ in range(4):
        r = send("NET %s WHISPER STATUS" % B, 2.6)
        t = " ".join(l for l in r.splitlines() if "from=%s " % B in l)
        m = re.search(r"phrases=(\d+) whispers=(\d+)", t)
        if m:
            return int(m.group(1)), int(m.group(2))
    return None


def scaled(path, gain, outdir):
    with wave.open(path) as w:
        n, params = w.getnframes(), w.getparams()
        data = array.array("h", w.readframes(n))
    out = array.array("h", (max(-32768, min(32767, int(v * gain))) for v in data))
    dst = os.path.join(outdir, os.path.basename(os.path.dirname(path)) + "_" + os.path.basename(path))
    with wave.open(dst, "wb") as w:
        w.setparams(params)
        w.writeframes(out.tobytes())
    return dst


def play(path):
    """Plays the WAV on the lab PC and returns what the gateway printed meanwhile."""
    p = subprocess.Popen(["powershell", "-NoProfile", "-Command",
                          "(New-Object System.Media.SoundPlayer '%s').PlaySync()" % os.path.abspath(path)])
    text = ""
    while p.poll() is None:
        text += s.read(65536).decode(errors="replace")
    text += read_for(a.tail)
    return text


files = []
for d in a.dirs:
    for n in sorted(os.listdir(d)):
        if n.endswith(".wav") and not n.startswith("cue") and n != "silence.wav":
            files.append(os.path.join(d, n))
out = []


def log(t):
    print(t, flush=True)
    out.append(t)


tmp = tempfile.mkdtemp(prefix="whisperband_")
BASE = [("whisper.enabled", 1), ("whisper.aec", 0), ("whisper.listen_while_playing", 0), ("whisper.gain", 0),
        ("whisper.level_band", 0), ("whisper.timbre_check", 0)]
for k, v in BASE:
    setp(k, v)
log("settling %.0f s (the room's level)" % a.settle)
read_for(a.settle)

gain = a.gain
if gain <= 0:
    probes = [f for f in files if "whisper_normal_05m" in f or "whisper_05m" in f][:1]
    best = None
    for g in (2, 4, 8, 16):
        levels = []
        for f in probes:
            txt = play(scaled(f, g, tmp))
            levels += [int(x) for x in re.findall(r"WHISPER (?:heard|phrase)[^\n]*?level_p90=(\d+)", txt)]
        med = sorted(levels)[len(levels) // 2] if levels else 0
        log("gain %2d: %d phrase lines, median level_p90 %d" % (g, len(levels), med))
        if levels and (best is None or abs(med - 450) < abs(best[1] - 450)):
            best = (g, med)
    gain = best[0] if best else 8
log("gain %.0f" % gain)
scaled_files = [(os.path.relpath(f), scaled(f, gain, tmp)) for f in files]

SETTINGS = [("A default", [("whisper.level_band", 0), ("whisper.timbre_check", 0)]),
            ("B level_band", [("whisper.level_band", 1), ("whisper.timbre_check", 0)]),
            ("C timbre_check", [("whisper.level_band", 0), ("whisper.timbre_check", 1)]),
            ("D both", [("whisper.level_band", 1), ("whisper.timbre_check", 1)])]
results = {}
for name, params in SETTINGS:
    for k, v in params:
        setp(k, v)
    read_for(12)
    results[name] = {}
    log("== %s" % name)
    for label, path in scaled_files:
        before = status()
        play(path)
        after = status()
        if before and after:
            results[name][label] = (after[0] - before[0], after[1] - before[1])
        else:
            results[name][label] = None
        log("  %-60s phrases %s passed %s" % (label, *(results[name][label] or ("?", "?"))))

labels = [l for l, _ in scaled_files]
log("")
log("%-62s %s" % ("label (phrases judged / passed)", "  ".join("%-14s" % n for n, _ in SETTINGS)))
for l in labels:
    row = []
    for n, _ in SETTINGS:
        r = results[n].get(l)
        row.append("%-14s" % ("%d / %d" % r if r else "?"))
    log("%-62s %s" % (l, "  ".join(row)))
s.close()
if a.out:
    open(a.out, "w", encoding="utf-8").write("\n".join(out) + "\n")
