import serial, time, re, sys
node = serial.Serial(sys.argv[1], 115200, timeout=0.05); node.dtr=False; node.rts=True; time.sleep(0.1); node.rts=False
emi = serial.Serial(); emi.port="COM4"; emi.baudrate=115200; emi.timeout=0.05; emi.dtr=False; emi.rts=False; emi.open()
buf=""
def pump(t):
    global buf
    end=time.time()+t
    while time.time()<end:
        b=node.read(4096)
        if b: buf+=b.decode(errors="replace")
        emi.read(4096)
pump(12)
for i in range(10):
    emi.write(f"EMIT PULSES 5 25 10 trial={i+1} freq=3300 hop=2950,3200,3450,3700,3950\n".encode())
    time.sleep(float(sys.argv[2]) if len(sys.argv)>2 else 0.10)
    node.write(b"PARAM SET detection.scalar_onset_threshold 2000\n")
    pump(6)
buf=""; node.write(b"RB summary\n"); pump(2)
m=re.search(r"RB summary: pending=(\d+) accepted=(\d+) validPatterns=(\d+)", buf); print("summary:", m.groups() if m else buf[:200])
