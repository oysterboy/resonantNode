#!/usr/bin/env python3
"""A synthetic whisper set for Whispernet's detector (step 20.3, owner 2026-10-31: synthetic first, real recordings
later). Needs numpy and scipy.

A noise vocoder turns the speech of bench_audio.wav into whispering: the speech is split into bands, each band's
envelope (smoothed ~10 ms) modulates band-limited noise, so syllables, pauses and the spectral colour stay and the
voice (the pitch, the harmonics) goes, as in a whisper. Out of one source file it writes, at --rate:

  whisper_female.wav, whisper_male.wav   the two speech sections as whispers, at --whisper-db (quiet)
  speech_female.wav, speech_male.wav     the same speech, voiced, at --speech-db (a control: must be ignored)
  music.wav                              the drum and bass section (a control)
  fan.wav                                steady pink-ish noise, no modulation (a control)
  claps.wav                              loud short transients (a control: the loud bound)
  mixed.wav                              whisper, speech, fan, whisper over fan, in a row, with silence between

  python tools/bench/synthwhisper.py --src C:/Users/admin/Downloads/bench_audio.wav --out <dir> [--rate 24000]
      [--whisper-db -45] [--speech-db -25] [--seconds 30]

The levels are dBFS of the file; native-echoreplay maps a full-scale file to the mic's full scale, so -45 dBFS is
about a whisper at a metre on the desk's mic (the Emitter's click at ~1 m read about -50 dBFS there).
"""
import argparse
import os
import wave

import numpy as np
from scipy import signal

SECTIONS = {"music": (10.0, 110.0), "female": (115.0, 215.0), "male": (220.0, 320.0)}


def read_wav(path):
    with wave.open(path, "rb") as w:
        rate, n, ch, width = w.getframerate(), w.getnframes(), w.getnchannels(), w.getsampwidth()
        raw = w.readframes(n)
    dtype = {2: np.int16, 4: np.int32}[width]
    x = np.frombuffer(raw, dtype=dtype).astype(np.float64) / float(np.iinfo(dtype).max)
    return rate, x.reshape(-1, ch).mean(axis=1)


def write_wav(path, rate, x):
    y = np.clip(x, -1.0, 1.0)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes((y * 32767).astype("<i2").tobytes())


def rms_db(x):
    return 20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-12)


def at_db(x, db):
    return x * (10 ** ((db - rms_db(x)) / 20))


def vocode(speech, rate, rng, bands=16, lo=300.0, hi=None):
    hi = hi or min(7000.0, rate / 2 * 0.9)
    edges = np.geomspace(lo, hi, bands + 1)
    env_lp = signal.butter(2, 100.0, "low", fs=rate, output="sos")  # ~10 ms envelope
    noise = rng.standard_normal(len(speech))
    out = np.zeros_like(speech)
    for a, b in zip(edges[:-1], edges[1:]):
        bp = signal.butter(3, [a, b], "band", fs=rate, output="sos")
        env = signal.sosfiltfilt(env_lp, np.abs(signal.sosfiltfilt(bp, speech)))
        out += signal.sosfiltfilt(bp, noise) * np.maximum(env, 0.0)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--rate", type=int, default=24000)
    ap.add_argument("--whisper-db", type=float, default=-45.0)
    ap.add_argument("--speech-db", type=float, default=-25.0)
    ap.add_argument("--seconds", type=float, default=30.0, help="length taken from each section")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    rng = np.random.default_rng(1)
    src_rate, x = read_wav(a.src)
    x = signal.resample_poly(x, a.rate, src_rate)
    cut = {k: x[int(s * a.rate):int(min(e, s + a.seconds) * a.rate)] for k, (s, e) in SECTIONS.items()}
    gap = np.zeros(a.rate)  # 1 s of silence

    whisper = {k: at_db(vocode(cut[k], a.rate, rng), a.whisper_db) for k in ("female", "male")}
    speech = {k: at_db(cut[k], a.speech_db) for k in ("female", "male")}
    fan = signal.sosfilt(signal.butter(1, 1500.0, "low", fs=a.rate, output="sos"),
                         rng.standard_normal(int(a.seconds * a.rate)))
    fan = at_db(fan, a.whisper_db)
    claps = np.zeros(int(a.seconds * a.rate))
    for t in np.arange(1.0, a.seconds - 0.5, 2.0):
        i = int(t * a.rate)
        n = int(0.03 * a.rate)
        claps[i:i + n] = rng.standard_normal(n) * np.exp(-np.arange(n) / (0.006 * a.rate))
    claps = at_db(claps, a.speech_db + 10) if np.any(claps) else claps

    files = {
        "whisper_female.wav": whisper["female"], "whisper_male.wav": whisper["male"],
        "speech_female.wav": speech["female"], "speech_male.wav": speech["male"],
        "music.wav": at_db(cut["music"], a.speech_db), "fan.wav": fan, "claps.wav": claps,
    }
    files["mixed.wav"] = np.concatenate([gap, whisper["female"][:10 * a.rate], gap, speech["male"][:10 * a.rate], gap,
                                         fan[:10 * a.rate], gap,
                                         whisper["male"][:10 * a.rate] + fan[:10 * a.rate], gap])
    for name, y in files.items():
        write_wav(os.path.join(a.out, name), a.rate, y)
        print("%-20s %5.1f s  rms %6.1f dBFS" % (name, len(y) / a.rate, rms_db(y)))


if __name__ == "__main__":
    main()
