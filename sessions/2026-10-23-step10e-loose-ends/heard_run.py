import os, re, time, serial
exec(open(os.environ["TMP"] + "/persist_test.py").read().split("show(\"boot 1")[0])
e = serial.Serial(); e.port = "COM4"; e.baudrate = 115200; e.timeout = 0.05; e.dtr = False; e.rts = False; e.open()
reboot(); read(10)
cmd("RB log full")
print("before:", re.findall(r"heard\.\w+=\d+", cmd("RB STATUS", 2)), flush=True)
got = ""
for i in range(5):
    e.write(b"EMIT PULSES 5 25 10 trial=%d freq=3300 hop=2950,3200,3450,3700,3950\n" % i)
    got += read(2.5)
for l in got.splitlines():
    if l.startswith("RB verdict="): print(" ", l.strip())
print("after:", re.findall(r"heard\.\w+=\d+", cmd("RB STATUS", 2)), flush=True)
print("emitter said:", [l.strip() for l in e.read(4000).decode(errors="replace").splitlines()][:3])
e.close()
print("holding", flush=True)
while True:
    s.read(4096); time.sleep(0.5)
