import re, sys, time, serial
SECRETS = "include/wifi_secrets.h"
TOKEN = re.search(r'#define\s+OTA_TOKEN\s+"([^"]*)"', open(SECRETS, encoding="utf-8").read()).group(1)
BASE = "http://192.168.178.33:8070/"
s = serial.Serial(); s.port, s.baudrate, s.timeout = "COM11", 115200, 0.1; s.dtr = False; s.rts = False; s.open()
def talk(cmd, wait=2.0, until=None):
    s.reset_input_buffer(); s.write((cmd + "\n").encode()); end = time.time() + wait; t = ""
    while time.time() < end:
        t += s.read(16384).decode(errors="replace")
        if until and until in t: break
    return t
def reset():
    s.rts = True; time.sleep(0.2); s.rts = False
    end = time.time() + 8; t = ""
    while time.time() < end: t += s.read(16384).decode(errors="replace")
    return t
def ota(img):
    t = talk("OTA url=%s%s token=%s" % (BASE, img, TOKEN), 180, "OTA confirmed")
    return [l for l in t.splitlines() if "BUILD" in l or "PARAM load" in l or "confirmed" in l]
def show(t, keys):
    return [l.strip()[:120] for l in t.splitlines() if any(k in l for k in keys)]
print("## node -> echo"); print(ota("echo.bin"))
print(show(talk("PARAM GET echo.gain"), ["echo.gain"]))
print(show(talk("PARAM SET echo.gain 100"), ["status="])); print(show(talk("PARAM SAVE", 3), ["SAVE", "status"]))
t = reset(); print("after reset:", show(t, ["PARAM load"])); print(show(talk("PARAM GET echo.gain"), ["echo.gain"]))
print(show(talk("PARAM DUMP", 3), ["output.silent", "whisper."])[:3], "(nothing of the Node's or whisper's expected)")
print("## echo -> whisper"); print(ota("whisper.bin"))
print(show(talk("PARAM GET whisper.gain"), ["whisper.gain"])); print(show(talk("PARAM GET whisper.delay_ms"), ["whisper.delay_ms"]))
print(show(talk("PARAM SET whisper.gain 50"), ["status="])); print(show(talk("PARAM SAVE", 3), ["SAVE", "status"]))
t = reset(); print("after reset:", show(t, ["PARAM load"])); print(show(talk("PARAM GET whisper.gain"), ["whisper.gain"]))
print("## whisper -> node"); print(ota("node.bin"))
print(show(talk("PARAM GET output.silent"), ["output.silent"]))
s.close()
