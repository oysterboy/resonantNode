"""Whispernet's echo cancellation: the speaker's delay behind the mic from a WHISPER AEC CAL dump.

  python tools/bench/aeccal.py <log with the WHISPER_CAL lines> [...]

The board prints the mic (the delay line, raw with whisper.aec 0) and what the DuplexStream wrote for the same mic
indices, n samples each. Played sample k reaches the mic D samples later: mic[i] ~ ref[i - D]; the
cross-correlation's peak gives D, which is what whisper.aec_delay wants (the canceller's window then holds the
direct sound 32 samples in). The dump prints the board's current value (aec_delay=). Needs numpy.
"""
import base64
import re
import sys

import numpy as np


def dumps(path):
    head, mic, ref = None, [], []
    for line in open(path, encoding="utf-8", errors="replace"):
        text = line.rstrip("\n")
        k = text.find("WHISPER_CAL")
        if k < 0:
            continue
        text = text[k:]
        if text.startswith("WHISPER_CAL_BEGIN"):
            head, mic, ref = dict(re.findall(r"(\w+)=(\S+)", text)), [], []
        elif text.startswith("WHISPER_CAL_MIC ") and head is not None:
            mic.append(text.split(" ", 1)[1])
        elif text.startswith("WHISPER_CAL_REF ") and head is not None:
            ref.append(text.split(" ", 1)[1])
        elif text.startswith("WHISPER_CAL_END") and head is not None:
            m = np.frombuffer(b"".join(base64.b64decode(x) for x in mic), "<i2").astype(float)
            r = np.frombuffer(b"".join(base64.b64decode(x) for x in ref), "<i2").astype(float)
            yield head, m, r
            head = None


for path in sys.argv[1:]:
    for head, mic, ref in dumps(path):
        n = int(head["n"])
        m = mic - mic.mean()
        r = ref - ref.mean()
        if not np.any(r):
            print("reference silent: no replay in the window")
            continue
        corr = np.correlate(m, r, "full")  # index k: lag (k - (n - 1)): sum m[i + lag] * r[i]
        k = int(np.argmax(np.abs(corr)))
        delay = k - (n - 1)  # mic[i + delay] ~ ref[i]
        norm = abs(corr[k]) / (np.sqrt((m * m).sum() * (r * r).sum()) + 1e-9)
        rate = int(head["rate"])
        print("D=%d samples (%.1f ms)  corr=%.2f  board aec_delay=%s queue=%s  mic rms %.0f ref rms %.0f" % (
            delay, 1000.0 * delay / rate, norm, head.get("aec_delay"), head.get("queue"),
            np.sqrt((m * m).mean()), np.sqrt((r * r).mean())))
