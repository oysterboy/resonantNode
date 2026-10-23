import serial, time
exec(open(__import__("os").environ["TMP"] + "/persist_test.py").read().split("show(\"boot 1")[0])
show("set+save", cmd("PARAM SET output.hop_rotation 3") + cmd("PARAM SAVE", 3))
t = time.time(); show("boot", reboot())
show("status", cmd("RB STATUS", 2), keys=("drop", "overflow"))
show("reset", cmd("PARAM RESET", 3))
show("boot after reset", reboot())
show("silence", cmd("PARAM SET behavior.idle_enabled 0") + cmd("PARAM SET detection.scalar_onset_threshold 200000") + cmd("RB log off"))
print("holding E1 open", flush=True)
while True:
    s.read(4096); time.sleep(0.5)
