"""Drive drifttest on COM10: per config reinstall, settle, capture twice; save logs."""
import os
import sys
import time

import serial

OUT = sys.argv[1]
SETTLE_S = float(sys.argv[2]) if len(sys.argv) > 2 else 10.0
CONFIGS = [
    ("A_ours_16k_align", "CFG rate=16000 align=1 dmacount=3 dmalen=128"),
    ("B_echo_48k_noalign", "CFG rate=48000 align=0 dmacount=4 dmalen=128"),
    ("C_48k_align", "CFG rate=48000 align=1 dmacount=4 dmalen=128"),
    ("D_16k_noalign", "CFG rate=16000 align=0 dmacount=3 dmalen=128"),
    ("A2_ours_16k_align", "CFG rate=16000 align=1 dmacount=3 dmalen=128"),
]
os.makedirs(OUT, exist_ok=True)
s = serial.Serial()
s.port = "COM10"
s.baudrate = 921600
s.timeout = 0.05
s.dtr = False
s.rts = False
s.open()
s.rts = True; time.sleep(0.1); s.rts = False
time.sleep(1.5)
print(s.read(65536).decode("utf-8", "replace").strip().splitlines()[-2:])


def until(marker, timeout):
    buf = b""
    end = time.time() + timeout
    while time.time() < end:
        buf += s.read(1 << 16)
        if marker in buf:
            # read the rest of the marker line
            while not buf.endswith(b"\n"):
                buf += s.read(256)
            return buf
    return buf


for name, cfg in CONFIGS:
    s.write((cfg + "\n").encode())
    print(name, until(b"CFG_OK", 5).decode("utf-8", "replace").strip().splitlines()[-1])
    time.sleep(SETTLE_S)
    for k in (1, 2):
        s.read(1 << 20)
        s.write(f"CAP n=16384 label={name}_{k}\n".encode())
        data = until(b"RAW_SUMMARY", 60)
        path = os.path.join(OUT, f"{name}_{k}.log")
        open(path, "wb").write(data.replace(b"\r\n", b"\n"))
        print("  ", path, len(data), data.strip().splitlines()[-1].decode())
        time.sleep(3)
s.close()
