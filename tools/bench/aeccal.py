"""Whispernet's echo cancellation: the true speaker -> mic offset from a WHISPER AEC CAL dump.

  python tools/bench/aeccal.py <log with the WHISPER_CAL lines> [...]

The board prints the mic (the delay line, raw with whisper.aec 0) and what it played (the output's tap), 4096
samples each, with their indices: mic sample i is RX index rx_end - n + i, played sample j is TX index tx_end - n +
j. The cross-correlation's peak d says the mic hears TX index (RX index + c), c = tx_end - rx_end + d. The
canceller's window starts 32 samples newer than that (its lead), so the offset it should use is c + 32; the dump
prints the board's own estimate (offset=). Needs numpy.
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
        corr = np.correlate(m, r, "full")  # index k <-> lag d = k - (n - 1): mic[i] ~ ref[i - d]
        k = int(np.argmax(np.abs(corr)))
        d = (n - 1) - k  # mic[i] pairs with ref[i + d]
        norm = abs(corr[k]) / (np.sqrt((m * m).sum() * (r * r).sum()) + 1e-9)
        c = int(head["tx_end"]) - int(head["rx_end"]) + d
        rate = int(head["rate"])
        print("peak lag d=%d (%.1f ms)  corr=%.2f  ->  TX = RX + %d; offset for the canceller %d (board: %s)" % (
            d, 1000.0 * d / rate, norm, c, c + 32, head.get("offset")))
        print("  mic rms %.0f  ref rms %.0f  (aec=%s)" % (np.sqrt((m * m).mean()), np.sqrt((r * r).mean()), head.get("aec")))
