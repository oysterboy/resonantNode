import os, re, time
exec(open(os.environ["TMP"] + "/persist_test.py").read().split("show(\"boot 1")[0])
def started(text):
    m = re.findall(r"chirpStarted=(\d+)", text); return int(m[-1]) if m else None
idle = ["PARAM SET behavior.idle_timeout_ms 2000", "PARAM SET behavior.idle_timeout_variation_ms 0", "RB log off"]
show("boot", reboot(), keys=("PARAM load", "RB silent", "RB node name"))
read(12)  # startup baseline to Active
out = "".join(cmd(c) for c in idle)
show("silent on", cmd("PARAM SET output.silent 1") + cmd("RB CALL"))
a = started(cmd("RB summary")); read(12); b = started(cmd("RB summary"))
print("silent 12 s: chirpStarted", a, "->", b, flush=True)
show("status", cmd("RB STATUS", 2), keys=())
print("  ", re.search(r"silent=\d", cmd("RB STATUS", 2)).group(0))
show("silent off", cmd("PARAM SET output.silent 0"))
a = started(cmd("RB summary")); read(12); b = started(cmd("RB summary"))
print("audible 12 s: chirpStarted", a, "->", b, flush=True)
show("save silent only", cmd("PARAM RESET", 3) + cmd("PARAM SET output.silent 1") + cmd("PARAM SAVE", 3))
show("boot with saved silent", reboot(), keys=("PARAM load", "RB silent", "RB node name"))
read(12); out = "".join(cmd(c) for c in idle)
a = started(cmd("RB summary")); read(12); b = started(cmd("RB summary"))
print("saved silent 12 s after boot: chirpStarted", a, "->", b, flush=True)
print("holding E1 open (store keeps output.silent=1)", flush=True)
while True:
    s.read(4096); time.sleep(0.5)
