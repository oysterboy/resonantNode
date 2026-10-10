"""R1 Node checks on one board: quiet boot + own idle chirps vs verdicts.

Holds the other board in reset (EN via RTS) so it stays silent, resets the
node, sends logging commands, and records everything for `secs` seconds.
"""
import sys, time, serial

node_port, quiet_port, out, secs = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])

q = serial.Serial(quiet_port, 115200)
q.dtr = False
q.rts = True  # EN low: other board held in reset, silent

s = serial.Serial(node_port, 115200, timeout=0.2)
s.dtr = False
s.rts = True
time.sleep(0.1)
s.rts = False

t0 = time.time()
sent = False
with open(out, "w", encoding="utf-8") as f:
    while time.time() - t0 < secs:
        line = s.readline().decode("utf-8", "replace").rstrip()
        if line:
            f.write(f"{time.time() - t0:8.2f} {line}\n")
            f.flush()
        if not sent and time.time() - t0 > 2.0:
            for c in ("RB log full", "RB debug events"):
                s.write((c + "\n").encode())
                time.sleep(0.2)
            sent = True

s.close()
q.rts = False  # release the other board
q.close()
