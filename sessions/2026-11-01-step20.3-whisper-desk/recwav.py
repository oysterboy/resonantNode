"""WHISPER_REC blocks in board logs (wchain.py output) -> WAV files plus an index.

  recwav.py <dir with E*.log> [--out DIR]
"""
import base64
import glob
import os
import re
import sys
import wave

d = sys.argv[1]
out = sys.argv[3] if len(sys.argv) > 3 and sys.argv[2] == "--out" else os.path.join(d, "rec")
os.makedirs(out, exist_ok=True)
for log in sorted(glob.glob(os.path.join(d, "E*.log"))):
    board = os.path.splitext(os.path.basename(log))[0]
    head, data, t = None, [], 0.0
    for line in open(log, encoding="utf-8", errors="replace"):
        parts = line.rstrip("\n").split(" ", 1)
        if len(parts) < 2:
            continue
        ts, text = parts
        if text.startswith("WHISPER_REC_BEGIN"):
            head, data, t = text, [], float(ts)
        elif text.startswith("WHISPER_REC ") and head:
            data.append(text[len("WHISPER_REC "):].strip())
        elif text.startswith("WHISPER_REC_END") and head:
            raw = b"".join(base64.b64decode(x) for x in data)
            seq = re.search(r"seq=(\d+)", head).group(1)
            rate = int(re.search(r"rate=(\d+)", head).group(1))
            name = "%s_rec%s.wav" % (board, seq)
            with wave.open(os.path.join(out, name), "wb") as w:
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(rate)
                w.writeframes(raw)
            print("%-14s t=%7.1f %5.2f s  %s" % (name, t, len(raw) / 2 / rate, head[len("WHISPER_REC_BEGIN "):]))
            head = None
