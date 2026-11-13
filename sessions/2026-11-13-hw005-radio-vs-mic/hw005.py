"""HW-005 on R1: the node's own radio against its mic (issue #99). R1 on USB (port restarts it on open), E0 gateway.
Phases: L1 listening radio on (quiet / traffic / quiet, call off, RB log full: every onset prints an RB verdict line),
S1 SELFTEST sets radio on (no traffic / traffic; strip at cap / dark), radio off (PARAM SAVE, restart by reopening):
S2 SELFTEST sets, L2 quiet listening; then PARAM RESET + restart, radio checked back by NET PING."""
import serial,time,re,threading,json,sys,os
OUT=sys.argv[1]; MIN=float(sys.argv[2]) if len(sys.argv)>2 else 5.0; N=int(sys.argv[3]) if len(sys.argv)>3 else 10
log=[]; res={"started":time.strftime("%Y-%m-%d %H:%M:%S"),"phase_min":MIN,"runs":N}
def P(*a):
    print(time.strftime("%H:%M:%S"),*a,flush=True)
def save():
    json.dump(res,open(os.path.join(OUT,"hw005.json"),"w"),indent=1)
    open(os.path.join(OUT,"console.log"),"w",encoding="utf-8").write("\n".join(log))
def opn(name):
    s=serial.Serial(name,115200,timeout=0.2); s.dtr=False; s.rts=False; return s
def rd(s,t):
    x="";e=time.time()+t
    while time.time()<e: x+=s.read(4096).decode("utf-8","replace")
    return x
lock=threading.Lock()
def cmd(s,l,until,t):
    with lock:
        s.reset_input_buffer()
    s.write((l+"\n").encode()); t0=time.time(); x=""
    while time.time()<t0+t:
        x+=s.read(4096).decode("utf-8","replace")
        if re.search(until,x): time.sleep(0.3); x+=s.read(4096).decode("utf-8","replace"); break
    log.append(">> %s\n%s"%(l,x)); return x
K=("i2s.dropped_dma_buffers","i2s.overflows","i2s.read_errors","audio.block_us_max","net.tx_failed","net.rx","heap.free_min","heard.valid","output.started")
def status(r1):
    x=cmd(r1,"RB STATUS","BUILD role",6); return {k:int(m.group(1)) for k in K for m in [re.search(re.escape(k)+r"=(-?\d+)",x)] if m}
def row(x):
    a=re.search(r"check=amp result=(\w+)(?: reason=(\w+))?.*?snr_db=([\d.]+) snr_min_db=([\d.]+) snr_max_db=([\d.]+) tone_spread=([\d.]+)",x)
    fl=[int(f) for f in re.findall(r"SELFTEST_AMP n=\d+ floor=(\d+)",x)]
    tn=[int(f) for f in re.findall(r"SELFTEST_AMP n=\d+ floor=\d+ tone=(\d+)",x)]
    mic=re.search(r"check=mic result=(\w+).*?bit8_pct=([\d.]+)",x)
    return dict(mic=mic.groups() if mic else None, amp=a.groups() if a else None, floors=fl, tones=tn)
class Hammer:
    def __init__(s,gw): s.gw=gw; s.stop=threading.Event(); s.n=0; s.frames=0
    def run(s):
        while not s.stop.is_set():
            x=cmd(s.gw,"NET R1 RB STATUS","done parts",4); s.n+=1; s.frames+=x.count("from=R1")
    def __enter__(s): s.t=threading.Thread(target=s.run); s.t.start(); return s
    def __exit__(s,*a): s.stop.set(); s.t.join()
def listen(r1,gw,label,traffic):
    b=status(r1); h=None; t0=time.time()
    if traffic:
        with Hammer(gw) as h: x=rd(r1,MIN*60)
    else: x=rd(r1,MIN*60)
    log.append("## listen %s\n%s"%(label,x)); a=status(r1)
    v=re.findall(r"RB verdict=\S* .*?valid=(\d)",x)
    out=dict(label=label,traffic=traffic,seconds=round(time.time()-t0),verdicts=len(v),valid=v.count("1"),
             hammer=dict(requests=h.n,frames=h.frames) if h else None,
             delta={k:a[k]-b[k] for k in b if k in a})
    P("listen",label,"verdicts",out["verdicts"],"valid",out["valid"],"hammer",out["hammer"],"delta",out["delta"]); return out
def stset(r1,gw,label,light,traffic):
    cmd(r1,light,"LIGHT (ok|ERR)",3); rows=[]
    ctx=Hammer(gw) if traffic else None
    if ctx: ctx.__enter__()
    try:
        for i in range(N):
            r=row(cmd(r1,"SELFTEST","SELFTEST_SUMMARY",45)); rows.append(r)
            P("selftest",label,i+1,r["mic"],r["amp"],r["floors"])
            cmd(r1,light,"LIGHT (ok|ERR)",3)
    finally:
        if ctx: ctx.__exit__()
    return dict(label=label,light=light,traffic=traffic,rows=rows,hammer=dict(requests=ctx.n,frames=ctx.frames) if ctx else None)
def boot(r1=None):
    if r1: r1.close(); time.sleep(1)
    r1=opn("COM3"); x=rd(r1,6); log.append("## boot\n"+x)
    b=re.search(r"BUILD role=\S+ git=\S+ date=.*? board=\S+",x); p=re.search(r"PARAM load stored=\d+[^\n]*",x); n=re.search(r"RB net started=\d",x)
    P("boot",b.group(0) if b else None,"|",p.group(0) if p else None,"|",n.group(0) if n else "net line missing"); return r1,(b.group(0) if b else None,p.group(0) if p else None,n.group(0) if n else None)
gw=opn("COM12"); rd(gw,3)
r1,res["boot_radio_on"]=boot()
x=cmd(gw,"NET * PARAM SET output.silent 1","done parts",8); res["muted"]=sorted(set(re.findall(r"from=(\w+) .*?new=1",x)))
P("muted",res["muted"])
cmd(r1,"PARAM SET output.silent 0","status=",3); cmd(r1,"PARAM SET behavior.call_enabled 0","status=",3)
cmd(r1,"LIGHT off","LIGHT ok",3); cmd(r1,"RB log full","RB",3)
res["L1"]=[]
for lab,tr in (("quiet1",False),("traffic",True),("quiet2",False)):
    res["L1"].append(listen(r1,gw,lab,tr)); save()
res["S1"]=[]
for light,tr in (("LIGHT 100",False),("LIGHT 100",True),("LIGHT off",False),("LIGHT off",True)):
    res["S1"].append(stset(r1,gw,"radio_on "+light+(" traffic" if tr else ""),light,tr)); save()
# radio off
cmd(r1,"PARAM SET node.radio_enabled 0","status=",3); cmd(r1,"PARAM SAVE","status=",5)
r1,res["boot_radio_off"]=boot(r1)
cmd(r1,"PARAM SET behavior.call_enabled 0","status=",3); cmd(r1,"LIGHT off","LIGHT ok",3); cmd(r1,"RB log full","RB",3)
res["S2"]=[]
for light in ("LIGHT 100","LIGHT off"):
    res["S2"].append(stset(r1,None,"radio_off "+light,light,False)); save()
res["L2"]=[listen(r1,None,"radio_off quiet",False)]; save()
# restore
cmd(r1,"LIGHT off","LIGHT ok",3); x=cmd(r1,"PARAM RESET","store=",5); log.append(x)
r1,res["boot_restored"]=boot(r1)
time.sleep(3); x=cmd(gw,"NET R1 NET PING","done parts",8); res["restored_ping"]=bool(re.search(r"from=R1",x))
P("restored, radio answers:",res["restored_ping"]); res["ended"]=time.strftime("%Y-%m-%d %H:%M:%S"); save(); P("saved")
