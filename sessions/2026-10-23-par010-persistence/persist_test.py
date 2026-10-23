import serial, time, sys
s = serial.Serial()
s.port = "COM8"; s.baudrate = 115200; s.timeout = 0.05
s.dtr = False; s.rts = False
s.open()

def reboot():
    s.rts = True; time.sleep(0.1); s.rts = False
    return read(8)

def read(sec):
    end = time.time() + sec; buf = b""
    while time.time() < end:
        buf += s.read(4096)
    return buf.decode(errors="replace")

def cmd(c, sec=1.5):
    s.write((c + "\n").encode()); return read(sec)

def show(title, text, keys=("PARAM", "RB node", "hop_check", "plan_check")):
    print("=====", title)
    for line in text.splitlines():
        if any(k in line for k in keys):
            print(" ", line.strip())

show("boot 1 (fresh)", reboot())
show("set", cmd("PARAM SET behavior.idle_enabled 0") + cmd("PARAM SET behavior.rest_ms 6000") + cmd("PARAM SET output.hop_rotation 3"))
show("save", cmd("PARAM SAVE", 3))
show("save again", cmd("PARAM SAVE", 3))
show("boot 2 (after save)", reboot())
show("get", cmd("PARAM GET behavior.rest_ms") + cmd("PARAM GET output.hop_rotation") + cmd("PARAM GET behavior.idle_enabled"))
show("set unsaved", cmd("PARAM SET behavior.rest_ms 7000"))
show("load", cmd("PARAM LOAD", 2) + cmd("PARAM GET behavior.rest_ms"))
show("reset", cmd("PARAM RESET", 3) + cmd("PARAM GET behavior.rest_ms") + cmd("PARAM GET output.hop_rotation"))
show("boot 3 (after reset)", reboot())
show("silence again (not saved)", cmd("PARAM SET behavior.idle_enabled 0") + cmd("PARAM SET detection.scalar_onset_threshold 200000") + cmd("RB log off"))
print("holding E1 open", flush=True)
while True:
    s.read(4096); time.sleep(0.5)
