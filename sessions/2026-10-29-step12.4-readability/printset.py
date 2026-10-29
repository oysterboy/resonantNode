"""12.4.6 printer set: the Node's status printers (boot, RB STATUS, RB summary, the PARAM SET checks) on one Node and
the Analyzer's SEQ system / inspect / source output. One log per part; compared by key order per line type.

  python printset.py <node port> <analyzer port> <outdir>
"""
import os
import sys
import time

import serial

node_port, an_port, out = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(out, exist_ok=True)


def op(p):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = p, 115200, 0.05
    s.dtr = False
    s.rts = False
    s.open()
    return s


def pump(s, sec):
    buf = ""
    end = time.time() + sec
    while time.time() < end:
        d = s.read(65536)
        if d:
            buf += d.decode(errors="replace")
    return buf


node = op(node_port)
pump(node, 0.5)
node.rts = True
time.sleep(0.1)
node.rts = False
log = pump(node, 30)
for cmd, wait in (("RB STATUS", 1.5), ("RB summary", 1.5), ("PARAM GET detection.shape_tail_ms", 1.0)):
    node.write((cmd + "\n").encode())
    log += pump(node, wait)
tail = [l for l in log.splitlines() if "path=detection.shape_tail_ms" in l]
value = tail[-1].split("value=")[1].split()[0] if tail and "value=" in tail[-1] else None
if value is not None:
    for cmd in ("PARAM SET detection.shape_tail_ms " + value, "PARAM GET output.silent"):
        node.write((cmd + "\n").encode())
        log += pump(node, 1.5)
    sil = [l for l in log.splitlines() if "path=output.silent" in l]
    sv = sil[-1].split("value=")[1].split()[0] if sil and "value=" in sil[-1] else "1"
    node.write(("PARAM SET output.silent " + sv + "\n").encode())
    log += pump(node, 1.5)
open(os.path.join(out, "node.log"), "w", encoding="utf-8").write(log)
node.close()

an = op(an_port)
pump(an, 0.5)
alog = ""
for mode in ("system", "inspect", "source"):
    an.write(("SEQ start profile=TonalPulseScalar tries=3 band=2950-3950 mode=%s when=all verbose=1\n" % mode).encode())
    alog += pump(an, 16)
open(os.path.join(out, "analyzer.log"), "w", encoding="utf-8").write(alog)
an.close()
print("node lines %d, analyzer lines %d" % (len(log.splitlines()), len(alog.splitlines())))
