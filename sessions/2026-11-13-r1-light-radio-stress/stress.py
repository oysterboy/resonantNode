import serial,time,re,threading,json,sys
r1=serial.Serial("COM3",115200,timeout=0.2); r1.dtr=False; r1.rts=False
gw=serial.Serial("COM12",115200,timeout=0.2); gw.dtr=False; gw.rts=False
log=[]
def rd(s,t):
    x="";e=time.time()+t
    while time.time()<e: x+=s.read(4096).decode("utf-8","replace")
    return x
def cmd(s,l,until,t):
    s.reset_input_buffer(); s.write((l+"\n").encode()); t0=time.time(); x=""
    while time.time()<t0+t:
        x+=s.read(4096).decode("utf-8","replace")
        if re.search(until,x): time.sleep(0.3); x+=s.read(4096).decode("utf-8","replace"); break
    log.append(">> %s\n%s"%(l,x)); return x
K=("i2s.dropped_dma_buffers","i2s.overflows","i2s.read_errors","audio.block_us_max","net.tx_failed","net.rx","heap.free_min")
def status():
    x=cmd(r1,"RB STATUS","BUILD role",6); return {k:int(m.group(1)) for k in K for m in [re.search(re.escape(k)+r"=(-?\d+)",x)] if m}
def row(x):
    a=re.search(r"check=amp result=(\w+)(?: reason=(\w+))?.*?snr_min_db=([\d.]+) snr_max_db=([\d.]+) tone_spread=([\d.]+)",x)
    fl=[int(f) for f in re.findall(r"SELFTEST_AMP n=\d+ floor=(\d+)",x)]
    tn=[int(f) for f in re.findall(r"SELFTEST_AMP n=\d+ floor=\d+ tone=(\d+)",x)]
    mic=re.search(r"check=mic result=(\w+)",x)
    return dict(mic=mic.group(1) if mic else None, amp=a.groups() if a else None, floors=fl, tones=tn)
stop=threading.Event(); hammer={"sent":0,"replies":0}
def ham():
    while not stop.is_set():
        x=cmd(gw,"NET R1 RB STATUS","done parts",4); hammer["sent"]+=1; hammer["replies"]+=x.count("from=R1")
rd(r1,4); rd(gw,3)
print(cmd(r1,"BUILD","BUILD role",3).count("BUILD"), "build ok", flush=True)
cmd(r1,"PARAM SET behavior.call_enabled 0","status=",3)
out={}
for light in ["LIGHT 100","LIGHT off"]:
    cmd(r1,light,"LIGHT ok",3)
    res={"before":status()}
    # A: USB SELFTEST while E0 hammers R1 with RB STATUS (R1 transmits during the measurement)
    hammer.update(sent=0,replies=0); stop.clear(); th=threading.Thread(target=ham); th.start()
    res["usb_under_tx"]=[]
    for i in range(5):
        r=row(cmd(r1,"SELFTEST","SELFTEST_SUMMARY",45)); res["usb_under_tx"].append(r)
        print(time.strftime("%H:%M:%S"),light,"USB+TX",r["mic"],r["amp"],r["floors"],flush=True)
    stop.set(); th.join(); res["hammer"]=dict(hammer)
    print("  hammer:",hammer,flush=True)
    # B: SELFTEST by radio from E0
    res["radio"]=[]
    for i in range(5):
        r=row(cmd(gw,"NET R1 SELFTEST","SELFTEST_SUMMARY",60)); res["radio"].append(r)
        print(time.strftime("%H:%M:%S"),light,"RADIO ",r["mic"],r["amp"],r["floors"],flush=True)
    res["after"]=status()
    res["delta"]={k:res["after"][k]-res["before"][k] for k in res["before"] if k in res["after"]}
    print("  delta:",res["delta"],flush=True)
    out[light]=res
cmd(r1,"LIGHT off","LIGHT ok",3); cmd(r1,"PARAM SET behavior.call_enabled 1","status=",3)
json.dump(out,open(sys.argv[1]+"/stress.json","w"),indent=1)
open(sys.argv[1]+"/console.log","w",encoding="utf-8").write("\n".join(log))
print("saved")
