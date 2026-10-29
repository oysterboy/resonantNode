import re, time, serial, os
D = "bench/sessions/2026-10-29-step12.4-readability/raw_after_12410"
s = serial.Serial(); s.port, s.baudrate, s.timeout = "COM3", 115200, 0.05; s.dtr = False; s.rts = False; s.open()
for name, cmd in (("feat_pre5_wrap", "RAW trigger f=3300 dur=100 pre=5 post=300 mode=feat"),
                  ("both_pre5_wrap", "RAW trigger f=3300 dur=100 pre=5 post=300 mode=both"),
                  ("pcm_pre5_wrap", "RAW trigger f=3300 dur=100 pre=5 post=200 mode=pcm")):
    s.write((cmd + "\n").encode()); buf = ""; end = time.time() + 40
    while time.time() < end:
        buf += s.read(65536).decode(errors="replace")
        if "RAW_SUMMARY" in buf: end = min(end, time.time() + 0.5)
    open(os.path.join(D, name + ".log"), "w", encoding="utf-8").write(buf)
    rows = [l for l in buf.splitlines() if l[:1].isdigit()]
    ms = [int(l.split(",")[0]) for l in rows]
    hdr = re.search(r"pre_samples=(\d+) post_samples=(\d+).*", buf)
    print(name, len(rows), "backwards", sum(1 for a, b in zip(ms, ms[1:]) if b < a), hdr.group(0)[:40] if hdr else "-",
          "pre span ms", (ms[79] - ms[0]) if len(ms) > 80 else "-", "first gap", ms[80] - ms[79] if len(ms) > 81 else "-")
    time.sleep(1)
s.close()
