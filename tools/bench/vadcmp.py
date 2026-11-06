"""Whisper detection compared on the labelled recordings (bench/sessions/2026-11-05-labels-c920*, one file per kind):
the fremd detectors WebRTC VAD (modes 0 and 3) and Silero VAD (the ONNX model) against Whispernet's chain
(native-whisperreplay --adaptive, a gain sweep), per file: share of time flagged and events.

  vadcmp.py --silero silero_vad.onnx --replay <program.exe of native-whisperreplay>
Needs numpy<2, scipy, webrtcvad-wheels, onnxruntime (a venv; not in the repo's requirements).
Result and reading: docs/research/whispernet.md, "Whisper detection against other detectors"."""
import glob, os, re, subprocess, sys, wave
import numpy as np
import onnxruntime as ort
import scipy.signal as sg
import webrtcvad

import argparse
ap = argparse.ArgumentParser()
ap.add_argument("--silero", required=True)
ap.add_argument("--replay", required=True)
ap.add_argument("--out", default="vadcmp.json")
A = ap.parse_args()
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..") + "/"
E = A.replay  # "C:/Users/admin/AppData/Local/Temp/claude/c----DATA-Nodes-resonantNode/06452682-0eaa-484f-8ae6-05333f655030/scratchpad/pio-build/native-whisperreplay/program.exe"
SETS = ["bench/sessions/2026-11-05-labels-c920", "bench/sessions/2026-11-05-labels-c920-2"]

def load16k(p):
    w = wave.open(p); sr = w.getframerate(); x = np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(np.float32)
    if w.getnchannels() > 1: x = x.reshape(-1, w.getnchannels()).mean(1)
    y = sg.resample_poly(x, 16000, sr)
    return np.clip(y, -32768, 32767)

def events(flags):
    f = np.asarray(flags, bool); return int(((f[1:] & ~f[:-1]).sum()) + (1 if f[:1].any() else 0))

def webrtc(y, mode):
    v = webrtcvad.Vad(mode); n = 480  # 30 ms
    fl = [v.is_speech(y[i:i+n].astype(np.int16).tobytes(), 16000) for i in range(0, len(y) - n, n)]
    return np.mean(fl), events(fl)

sess = ort.InferenceSession(A.silero, providers=["CPUExecutionProvider"])
def silero(y, thr=0.5):
    st = np.zeros((2, 1, 128), np.float32); ctx = np.zeros((1, 64), np.float32); fl = []
    x = (y / 32768.0).astype(np.float32)
    for i in range(0, len(x) - 512, 512):
        inp = np.concatenate([ctx, x[i:i+512][None]], 1)
        out, st = sess.run(None, {"input": inp, "state": st, "sr": np.array(16000, np.int64)})
        ctx = inp[:, -64:]; fl.append(float(out[0][0]) > thr)
    return np.mean(fl), events(fl)

def ours(path, room, db):
    o = subprocess.run([E, "--plan", "whisper", "--rate", "24000", "--level-db", str(db), "--adaptive", "--room", room, "--verbose", path],
                       capture_output=True, text=True).stdout
    m = re.search(r"phrases=(\d+) whispers=(\d+) \(([\d.]+) of ([\d.]+) s\)", o)
    return (int(m.group(1)), int(m.group(2)), float(m.group(3)))

rows = []
for d in SETS:
    room = R + d + "/silence.wav"
    for p in sorted(glob.glob(R + d + "/*.wav")):
        name = os.path.basename(d)[-2:] + "/" + os.path.basename(p)[:-4]
        y = load16k(p)
        r = {"name": name, "secs": len(y) / 16000}
        for m in (0, 3): r["rtc%d" % m] = webrtc(y, m)
        r["silero"] = silero(y)
        for db in (-36, -32, -28, -24): r["ours%+d" % db] = ours(p, room, db)
        rows.append(r); print(r["name"], flush=True, file=sys.stderr)
import json; json.dump(rows, open(A.out, "w"))
print("%-34s %6s | %-11s %-11s %-11s | %s" % ("file", "secs", "rtc0 %/ev", "rtc3 %/ev", "silero %/ev", "ours phrases/passed adaptive at -36,-32,-28,-24 dB"))
for r in rows:
    f = lambda t: "%3d%%/%-3d" % (round(100 * t[0]), t[1])
    print("%-34s %6.0f | %-11s %-11s %-11s | %s" % (r["name"], r["secs"], f(r["rtc0"]), f(r["rtc3"]), f(r["silero"]),
          "  ".join("%d/%d" % (r["ours%+d" % db][0], r["ours%+d" % db][1]) for db in (-36, -32, -28, -24))))
