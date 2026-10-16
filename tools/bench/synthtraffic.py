"""Synthetic street-traffic test signal (stand-in until a real recording exists).

Deterministic (seeded) mono 16 bit WAV with:
  - rumble: low-passed brown noise (engines, tyre drone), swells as vehicles pass
  - roll: band-limited noise 500-3000 Hz (tyre/road noise), same swells, quieter
  - engine: harmonic series on a gliding 40-90 Hz fundamental, same swells
  - horns: car horn (two tones, odd harmonics reach the 2.8-3.8 kHz detector band),
    truck horn (low, long), bicycle bell (decaying 3.1 kHz tone, inside the band)
Levels are relative; set the playback level with tools/bench/buildaudio.py and
ambientlevel.py. Use a real recording when you have one: this only covers the
spectral families (low rumble, broadband roll, tonal horns), not real traffic.

  python tools/bench/synthtraffic.py --out traffic_synth.wav --seconds 150
"""
import argparse
import array
import math
import random
import sys
import wave

ap = argparse.ArgumentParser()
ap.add_argument("--out", required=True)
ap.add_argument("--seconds", type=float, default=150.0)
ap.add_argument("--rate", type=int, default=44100)
ap.add_argument("--seed", type=int, default=7)
a = ap.parse_args()
R = a.rate
N = int(a.seconds * R)
rnd = random.Random(a.seed)


def swell_envelope():
    """Passing vehicles: raised-cosine swells, 3-7 s long, every 5-11 s, floor 0.25."""
    env = [0.25] * N
    t = rnd.uniform(1, 4)
    while t < a.seconds:
        length = rnd.uniform(3.0, 7.0)
        peak = rnd.uniform(0.6, 1.0)
        i0, n = int(t * R), int(length * R)
        for k in range(n):
            if i0 + k >= N:
                break
            env[i0 + k] = max(env[i0 + k], 0.25 + (peak - 0.25) * 0.5 * (1 - math.cos(2 * math.pi * k / n)))
        t += rnd.uniform(5.0, 11.0)
    return env


env = swell_envelope()
out = [0.0] * N

# rumble: brown noise through a ~150 Hz one-pole low-pass
lp_a = math.exp(-2 * math.pi * 150 / R)
brown = lp = 0.0
for i in range(N):
    brown = 0.998 * brown + rnd.gauss(0, 1) * 0.05
    lp = lp_a * lp + (1 - lp_a) * brown
    out[i] += lp * 9.0 * env[i]

# roll: noise between ~500 Hz (high-pass) and ~3000 Hz (low-pass)
hp_a, lp2_a = math.exp(-2 * math.pi * 500 / R), math.exp(-2 * math.pi * 3000 / R)
x_prev = y_hp = lp2 = 0.0
for i in range(N):
    x = rnd.gauss(0, 1)
    y_hp = hp_a * (y_hp + x - x_prev)
    x_prev = x
    lp2 = lp2_a * lp2 + (1 - lp2_a) * y_hp
    out[i] += lp2 * 0.55 * env[i]

# engine: harmonics of a gliding fundamental, amplitude follows the swell
phase = 0.0
for i in range(N):
    f0 = 40 + 50 * env[i]
    phase += 2 * math.pi * f0 / R
    s = sum(math.sin(h * phase) / h for h in range(1, 9))
    out[i] += s * 0.25 * env[i]


def tone(freqs, start_s, dur_s, level, decay=None, odd_to=4000):
    i0, n = int(start_s * R), int(dur_s * R)
    for k in range(n):
        if i0 + k >= N:
            break
        t = k / R
        g = level * min(1.0, k / (0.01 * R), (n - k) / (0.02 * R))
        if decay:
            g *= math.exp(-t / decay)
        v = 0.0
        for f in freqs:
            h = 1
            while h * f <= odd_to:
                v += math.sin(2 * math.pi * h * f * t) / h
                h += 2
        out[i0 + k] += v * g


t = rnd.uniform(6, 14)
while t < a.seconds - 4:
    kind = rnd.random()
    if kind < 0.55:
        tone([420, 520], t, rnd.uniform(0.3, 1.2), 0.9)
    elif kind < 0.75:
        tone([220, 277], t, rnd.uniform(1.0, 2.2), 1.0)
    else:
        tone([3100], t, 0.6, 1.3, decay=0.18, odd_to=3200)
    t += rnd.uniform(9, 22)

peak = max(abs(v) for v in out)
scale = 0.8 * 32767 / peak
pcm = array.array("h", (int(max(-32767, min(32767, v * scale))) for v in out))
if sys.byteorder == "big":
    pcm.byteswap()
with wave.open(a.out, "wb") as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(R)
    w.writeframes(pcm.tobytes())
print("wrote %s (%.0f s)" % (a.out, a.seconds))
