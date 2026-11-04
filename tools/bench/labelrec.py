"""Labelled recordings from the lab PC's mic for Whispernet's whisper-vs-rustle question: a plan of sections (the
deskrun.py format: name, secs, cue, say), cue beeps before the cued ones, one continuous recording cut into one WAV
per section, then the features per label over its loud 1 s windows: the board's spectral flatness (BandFlatness's
8 bands, 300-10,000 Hz), the energy above 6 kHz, bumps per second and their rise time.

  python tools/bench/labelrec.py <plan.json> --out DIR [--device N] [--rate 24000] [--start-in 30] [--dry]
  python tools/bench/labelrec.py --analyse DIR

Needs sounddevice, numpy, scipy. The device: python -c "import sounddevice; print(sounddevice.query_devices())".
"""
import argparse
import json
import math
import os
import struct
import subprocess
import threading
import time
import wave

import numpy as np
from scipy import signal

ap = argparse.ArgumentParser()
ap.add_argument("plan", nargs="?")
ap.add_argument("--out", default="")
ap.add_argument("--device", type=int, default=None)
ap.add_argument("--rate", type=int, default=24000)
ap.add_argument("--start-in", type=float, default=30.0)
ap.add_argument("--dry", action="store_true")
ap.add_argument("--analyse", default="")
a = ap.parse_args()


def flatness_blocks(x, rate, block=128):
    centres = np.geomspace(300, 10000, 8)
    bands = []
    for f0 in centres:
        w0 = 2 * np.pi * f0 / rate
        alpha = np.sin(w0) / 5.0
        a0 = 1 + alpha
        bands.append(signal.lfilter([alpha / a0, 0, -alpha / a0], [1, -2 * np.cos(w0) / a0, (1 - alpha) / a0], x) ** 2)
    bands = np.array(bands)
    out = []
    for i in range(0, len(x) - block, block):
        e = bands[:, i:i + block].sum(axis=1) / centres + 1e-12
        out.append(np.exp(np.mean(np.log(e))) / np.mean(e))
    return np.array(out)


def features(x, rate, floor):
    """Per 1 s window louder than 2 x the room's floor (rms): board flatness (median of its blocks), HF share,
    bumps/s, median rise ms."""
    rows = []
    win = rate
    for i in range(0, len(x) - win, win // 2):
        w = x[i:i + win]
        if np.sqrt(np.mean(w * w)) < 2 * floor:
            continue  # quiet: not the label's sound
        fl = float(np.median(flatness_blocks(w, rate)))
        f, P = signal.welch(w, rate, nperseg=512)
        hf = float(P[f >= 6000].sum() / (P[f >= 100].sum() + 1e-12))
        env = signal.sosfiltfilt(signal.butter(2, 150, fs=rate, output="sos"), np.abs(w))[:: rate // 1000]
        pk, pr = signal.find_peaks(env, prominence=0.25 * np.percentile(env, 95), distance=60)
        rises = []
        for p, lb in zip(pk, pr["left_bases"]):
            seg = env[lb:p + 1]
            lo, hi = env[lb], env[p]
            rises.append(int(np.argmax(seg >= lo + 0.9 * (hi - lo)) - np.argmax(seg >= lo + 0.1 * (hi - lo))))
        rows.append((fl, hf, len(pk), float(np.median(rises)) if rises else np.nan))
    return np.array(rows) if rows else np.zeros((0, 4))


def load(path):
    with wave.open(path) as w:
        return w.getframerate(), np.frombuffer(w.readframes(w.getnframes()), "<i2").astype(float)


def room_floor(d, names):
    """The room's rms: the "silence" section's median 1 s rms, else the quietest section's 10th percentile."""
    def rms_windows(x, rate):
        return np.array([np.sqrt(np.mean(x[i:i + rate] ** 2)) for i in range(0, len(x) - rate, rate)])
    if "silence.wav" in names:
        rate, x = load(os.path.join(d, "silence.wav"))
        return float(np.median(rms_windows(x, rate)))
    vals = [np.percentile(rms_windows(*reversed(load(os.path.join(d, n)))), 10) for n in names]
    return float(min(vals))


def analyse(d):
    names = [n for n in sorted(os.listdir(d)) if n.endswith(".wav") and not n.startswith("cue")]
    floor = room_floor(d, names)
    print("room floor (rms): %.0f" % floor)
    print("%-22s %4s  %-18s %-18s %-14s %-14s" % ("label", "win", "flatness", "HF >6k", "bumps/s", "rise ms"))
    for name in names:
        if name == "silence.wav":
            continue
        rate, x = load(os.path.join(d, name))
        r = features(x, rate, floor)
        if len(r) == 0:
            print("%-22s    0  (nothing loud)" % name[:-4])
            continue
        f = lambda c: "%.2f (%.2f-%.2f)" % (np.nanmedian(r[:, c]), np.nanpercentile(r[:, c], 10),
                                           np.nanpercentile(r[:, c], 90))
        print("%-22s %4d  %-18s %-18s %-14s %-14s" % (name[:-4], len(r), f(0), f(1), "%.0f" % np.median(r[:, 2]),
                                                      "%.0f" % np.nanmedian(r[:, 3])))


if a.analyse:
    analyse(a.analyse)
    raise SystemExit

import sounddevice as sd  # noqa: E402  (only for recording)

plan = json.load(open(a.plan, encoding="utf-8"))
os.makedirs(a.out, exist_ok=True)
t = time.time() + a.start_in
schedule, cued = [], 0
for sec in plan["sections"]:
    cue = 0
    if sec.get("cue"):
        cued += 1
        cue = cued
    schedule.append(dict(sec, start=t, end=t + sec["secs"], cue_beeps=cue))
    t += sec["secs"]
json.dump(schedule, open(os.path.join(a.out, "schedule.json"), "w", encoding="utf-8"), indent=1)
for s in schedule:
    if s.get("say") or s["cue_beeps"]:
        print("%s-%s  %-22s %s%s" % (time.strftime("%H:%M:%S", time.localtime(s["start"])),
                                     time.strftime("%H:%M:%S", time.localtime(s["end"])), s["name"],
                                     ("(%d beeps) " % s["cue_beeps"]) if s["cue_beeps"] else "", s.get("say", "")))
if a.dry:
    raise SystemExit

for n in range(1, cued + 1):
    with wave.open(os.path.join(a.out, "cue%d.wav" % n), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(24000)
        fr = b""
        for k in range(n):
            fr += b"".join(struct.pack("<h", int(1500 * math.sin(2 * math.pi * 1000 * i / 24000))) for i in range(1920))
            fr += b"\x00\x00" * 4800
        w.writeframes(fr)

total = schedule[-1]["end"] - time.time() + 2
t0 = time.time()
rec = sd.rec(int(total * a.rate), samplerate=a.rate, channels=1, dtype="int16", device=a.device)


def cues():
    for s in schedule:
        if s["cue_beeps"]:
            while time.time() < s["start"] - 3:
                time.sleep(0.1)
            p = os.path.abspath(os.path.join(a.out, "cue%d.wav" % s["cue_beeps"]))
            subprocess.run(["powershell", "-NoProfile", "-Command",
                            "(New-Object System.Media.SoundPlayer '%s').PlaySync()" % p], check=False)
            print("%s  %s" % (time.strftime("%H:%M:%S"), s["name"]), flush=True)


threading.Thread(target=cues, daemon=True).start()
sd.wait()
x = rec[:, 0]
for s in schedule:
    if s["name"] == "pause":
        continue
    i0 = int((s["start"] - t0) * a.rate)
    i1 = int((s["end"] - t0) * a.rate)
    with wave.open(os.path.join(a.out, s["name"] + ".wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(a.rate)
        w.writeframes(x[max(0, i0):i1].astype("<i2").tobytes())
print("recorded; features:")
analyse(a.out)
