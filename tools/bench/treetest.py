"""Feasibility of research/whispernet.md's way 4: a small decision tree on per-phrase features (band excess over the
room, flatness, modulation, centroid, zero crossings) from the labelled recordings, whisper kinds against the others,
leave-one-kind-out. The phrases come from native-whisperreplay with the level window and the timbre check open.

  treetest.py <program.exe of native-whisperreplay>
Needs numpy<2, scipy, scikit-learn (a venv)."""
import sys
import glob, os, re, subprocess, wave, collections
import numpy as np, scipy.signal as sg
from sklearn.tree import DecisionTreeClassifier, export_text
E = sys.argv[1]
import os
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "bench", "sessions") + "/"
SETS = ["2026-11-05-labels-c920", "2026-11-05-labels-c920-2"]
BANDS = [(200, 700), (700, 1500), (1500, 3000), (3000, 4500), (4500, 8000)]
def load(p):
    w = wave.open(p); x = np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(np.float64)
    return x / 32768, w.getframerate()
def bandpow(seg, sr):
    f, P = sg.welch(seg, sr, nperseg=min(1024, len(seg)))
    return np.array([P[(f >= a) & (f < b)].sum() for a, b in BANDS]) + 1e-12, P, f
def feats(seg, sr, room):
    bp, P, f = bandpow(seg, sr)
    ex = 10 * np.log10(bp / room)
    tot = 10 * np.log10(bp.sum() / room.sum())
    env = np.abs(sg.hilbert(seg)); env = sg.resample_poly(env, 100, sr) if sr % 100 == 0 else env[::sr // 100]
    env = env - env.mean(); S = np.abs(np.fft.rfft(env * np.hanning(len(env)))) ** 2; fr = np.fft.rfftfreq(len(env), 0.01)
    mod = S[(fr >= 3) & (fr <= 8)].sum() / (S[(fr >= .5) & (fr <= 20)].sum() + 1e-12) if len(env) > 20 else 0
    flat = np.exp(np.mean(np.log(P[(f > 300) & (f < 8000)] + 1e-12))) / (np.mean(P[(f > 300) & (f < 8000)]) + 1e-12)
    cen = (P * f).sum() / (P.sum() + 1e-12)
    zcr = np.mean(np.abs(np.diff(np.sign(seg)))) / 2
    return list(ex) + [tot, mod, flat, cen, zcr, len(seg) / sr]
NAMES = ["ex200-700", "ex700-1.5k", "ex1.5-3k", "ex3-4.5k", "ex4.5-8k", "total_dB", "mod3-8Hz", "flat", "centroid", "zcr", "dur_s"]
X, y, grp = [], [], []
for d in SETS:
    room, sr0 = load(R + d + "/silence.wav"); roomp = bandpow(room, sr0)[0]
    for p in sorted(glob.glob(R + d + "/*.wav")):
        kind = os.path.basename(p)[:-4]
        if kind == "silence": continue
        kind = re.sub(r"_(05m|1m|2m)$", "", kind).replace("speech_soft", "speech")
        x, sr = load(p)
        o = subprocess.run([E, "--plan", "whisper", "--rate", "24000", "--level-db", "-32", "--adaptive", "--room-factor", "1.5", "--floor", "60", "--loud", "2500", "--flat-min", "0", "--flat-max", "1", "--room", R + d + "/silence.wav", "--verbose", p], capture_output=True, text=True).stdout
        for m in re.finditer(r"t_ms=(\d+) dur_ms=(\d+)", o):
            t, du = int(m.group(1)) / 1000 - len(room) / sr0, int(m.group(2)) / 1000
            a, b = int(t * sr), int((t + du) * sr)
            if a < 0 or b > len(x) or b - a < sr // 10: continue
            X.append(feats(x[a:b], sr, roomp)); y.append(1 if kind.startswith("whisper") else 0); grp.append(kind)
X, y, grp = np.array(X), np.array(y), np.array(grp)
print("phrases: whisper %d, other %d" % (y.sum(), (1 - y).sum()))
print("per kind:", dict(collections.Counter(grp)))
tp = tn = 0; per = {}
for k in sorted(set(grp)):
    te = grp == k; tr = ~te
    clf = DecisionTreeClassifier(max_depth=3, min_samples_leaf=4, class_weight="balanced", random_state=0).fit(X[tr], y[tr])
    pr = clf.predict(X[te]); per[k] = (int(pr.sum()), int(te.sum()), y[te][0])
for k, (hit, n, lab) in per.items():
    print("%-10s %-7s flagged as whisper %2d of %2d" % (k, "WHISPER" if lab else "noise", hit, n))
w_hit = sum(h for h, n, l in per.values() if l); w_n = sum(n for h, n, l in per.values() if l)
n_hit = sum(h for h, n, l in per.values() if not l); n_n = sum(n for h, n, l in per.values() if not l)
print("held-out kinds: whisper recall %d/%d (%.0f%%), noise falsely flagged %d/%d (%.0f%%)" % (w_hit, w_n, 100 * w_hit / w_n, n_hit, n_n, 100 * n_hit / n_n))
clf = DecisionTreeClassifier(max_depth=3, min_samples_leaf=4, class_weight="balanced", random_state=0).fit(X, y)
print(export_text(clf, feature_names=NAMES))
print("importances:", {n: round(v, 2) for n, v in zip(NAMES, clf.feature_importances_) if v > 0})
