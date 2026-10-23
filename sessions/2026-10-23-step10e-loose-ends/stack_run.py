import os, re, time
exec(open(os.environ["TMP"] + "/persist_test.py").read().split("show(\"boot 1")[0])
def sf(t):
    m = re.findall(r"stack_free_min_bytes=(\d+)", t); return m[-1] if m else "?"
b = reboot()
print("boot:", [l.strip() for l in b.splitlines() if "RB silent" in l or "PARAM load stored" in l])
read(10)
print("after boot + 10 s (startup rebase done):", sf(cmd("RB STATUS", 2)))
cmd("RB rebase", 4)
print("after RB rebase:", sf(cmd("RB STATUS", 2)))
cmd("RB PROFILE name=tonalpulse", 3)
print("after RB PROFILE:", sf(cmd("RB STATUS", 2)))
for c in ["PARAM SET output.silent 0", "PARAM SET behavior.idle_timeout_ms 2000", "PARAM SET behavior.idle_timeout_variation_ms 0", "RB log full"]:
    cmd(c)
t = read(12)
print("12 s of idle calls, log full:", sf(cmd("RB STATUS", 2)))
print("verdict line sample:", next((l.strip() for l in t.splitlines() if l.startswith("RB verdict=")), "none heard"))
st = cmd("RB STATUS", 2)
print("heard fields:", re.findall(r"heard\.\w+=\d+", st))
cmd("PARAM SET output.silent 1"); 
print("idle again silent; rebooting to the saved state", flush=True)
reboot(); print("holding", flush=True)
while True:
    s.read(4096); time.sleep(0.5)
