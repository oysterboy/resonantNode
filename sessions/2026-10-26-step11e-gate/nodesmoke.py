"""Step 11e gate, Node smoke on E1 (COM8) with E3 (COM4) as the Emitter."""
import re
import time

import serial


def open_port(p):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = p, 115200, 0.02
    s.dtr = False
    s.rts = False
    s.open()
    return s


node, emit = open_port("COM8"), open_port("COM4")
buf = ""


def pump(sec):
    global buf
    end = time.time() + sec
    while time.time() < end:
        d = node.read(8192)
        if d:
            buf += d.decode(errors="replace")
        emit.read(4096)


def cmd(c, sec=1.5):
    m = len(buf)
    node.write((c + "\n").encode())
    pump(sec)
    return buf[m:]


def show(title, text, keys):
    print("==", title)
    for l in text.splitlines():
        if any(k in l for k in keys):
            print("  ", l.strip()[:170])


m = len(buf)
node.rts = True
time.sleep(0.1)
node.rts = False
pump(12)
show("boot", buf[m:], ("BUILD", "PARAM load", "RB node name", "RB silent", "RB net", "hop_check", "plan_check", "RB PROFILE"))
show("RB PROFILE", cmd("RB PROFILE"), ("RB PROFILE",))
st = cmd("RB STATUS", 2)
print("== RB STATUS:", len(st.strip()), "chars;", " ".join(re.findall(r"(?:silent|output\.phase|i2s\.dropped_dma_buffers|net\.started|loop\.stack_free_min_bytes)=\S+", st)))

cmd("PARAM SET output.silent 0")
cmd("PARAM SET behavior.idle_enabled 0")  # only answers count
cmd("RB log full")
m = len(buf)
for i in range(10):
    emit.write(b"EMIT PULSES 5 25 10 trial=%d freq=3300 hop=2950,3200,3450,3700,3950\n" % (i + 1))
    pump(7)  # past the node's rest and refractory
t = buf[m:]
heard = len(re.findall(r"^RB verdict=.*valid=1", t, re.M))
answers = len(re.findall(r"BEH emit start .*source=heard_pattern", t))
print("== 10 hop trains from E3: valid verdicts %d, answers %d" % (heard, answers))

cmd("PARAM SET output.silent 1")
show("SAVE with output.silent 1", cmd("PARAM SAVE", 3), ("PARAM SAVE",))
show("PARAM RESET", cmd("PARAM RESET", 3), ("PARAM RESET", "RB silent"))
st = cmd("RB STATUS", 2)
print("== after RESET:", " ".join(re.findall(r"(?:silent|behavior_idle)=\S+", st)))
cmd("RB log full")
m = len(buf)
emit.write(b"EMIT PULSES 5 25 10 trial=99 freq=3300 hop=2950,3200,3450,3700,3950\n")
pump(4)
print("== one train after RESET: answers %d (the node sounds again)" % len(re.findall(r"BEH emit start .*source=heard_pattern", buf[m:])))
# leave it silent again, stored
cmd("PARAM SET output.silent 1")
show("restore silent (stored)", cmd("PARAM SAVE", 3), ("PARAM SAVE",))
node.close()
emit.close()
