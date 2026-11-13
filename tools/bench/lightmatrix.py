"""Light x I2S x radio matrix on one Node (issues #94 check 2 and #99).

One run covers one firmware state (the image and R1's radio setting are fixed by whoever flashed it; the
setup goes into the session as --label text). Inside that state it walks the light cells over USB:

  light=idle   no LIGHT frame (strip dark)
  light=white  LIGHT 100 held (the strip's white under the 0.5 A cap)
  light=alt    LIGHT alt held (even red / odd blue)

For every cell: N x SELFTEST with the idle call off (mic pass and bit8, amp pass, SNR, tone spread, per-chirp
floor and tone), then a listen phase with the idle call on (RB STATUS before and after: i2s drops / overflows /
read errors, audio.block_us_max, heap.free_min, net.tx_failed, heard.valid, output.started). A gateway port is
optional: R1's NET PING counters at the start and the end of each cell.

  python tools/bench/lightmatrix.py --r1 COM4 --gw COM6 --label "lr2 light-on image, radio on" --runs 10

Output: bench/sessions/<date>-lightmatrix-<slug>/lightmatrix.json and console.log (a run without its setup and
firmware hash is not evidence: the BUILD banner is read first and stored). The idle call and the light are
restored at the end (call_enabled 1, LIGHT off).
"""
import argparse
import datetime
import json
import os
import re
import statistics
import sys
import time

import serial

KEYS = ("i2s.dropped_dma_buffers", "i2s.overflows", "i2s.read_errors", "audio.block_us_max", "heap.free_min",
        "net.tx_failed", "net.rx", "heard.valid", "output.started", "behavior.declined")


class Port:
    def __init__(self, name):
        self.s = serial.Serial(name, 115200, timeout=0.2)
        self.s.dtr = False
        self.s.rts = False
        self.log = []

    def drain(self, seconds=0.3):
        end = time.time() + seconds
        text = ""
        while time.time() < end:
            text += self.s.read(4096).decode("utf-8", "replace")
        self.log.append(text)
        return text

    def cmd(self, line, until=None, timeout=5.0):
        self.s.reset_input_buffer()
        self.s.write((line + "\n").encode())
        end = time.time() + timeout
        text = ""
        while time.time() < end:
            text += self.s.read(4096).decode("utf-8", "replace")
            if until and re.search(until, text):
                time.sleep(0.4)
                text += self.s.read(4096).decode("utf-8", "replace")
                break
        self.log.append(">> " + line + "\n" + text)
        return text


def selftest_row(text):
    row = {"mic": None, "bit8": None, "amp": None, "reason": None, "snr_min": None, "snr_max": None,
           "spread": None, "floors": [], "tones": []}
    m = re.search(r"check=mic result=(\w+).*?bit8_pct=([\d.]+)", text)
    if m:
        row["mic"], row["bit8"] = m.group(1), float(m.group(2))
    m = re.search(r"check=amp result=(\w+)(?: reason=(\w+))?.*?snr_min_db=([\d.]+) snr_max_db=([\d.]+) "
                  r"tone_spread=([\d.]+)", text)
    if m:
        row["amp"], row["reason"] = m.group(1), m.group(2)
        row["snr_min"], row["snr_max"], row["spread"] = float(m.group(3)), float(m.group(4)), float(m.group(5))
    for n in re.finditer(r"SELFTEST_AMP n=\d+ floor=(\d+) tone=(\d+)", text):
        row["floors"].append(int(n.group(1)))
        row["tones"].append(int(n.group(2)))
    return row


def status_keys(text):
    out = {}
    for k in KEYS:
        m = re.search(re.escape(k) + r"=(-?\d+)", text)
        if m:
            out[k] = int(m.group(1))
    return out


def ping_counters(gw, name):
    if gw is None:
        return None
    text = gw.cmd("NET %s NET PING" % name, until=r"done parts", timeout=10)
    m = re.search(r"NET node=%s .*?rx=(\d+) rejected=(\d+) queue_full=(\d+) tx=(\d+) tx_failed=(\d+) outbox_full=(\d+)"
                  % name, text)
    return None if not m else dict(zip(("rx", "rejected", "queue_full", "tx", "tx_failed", "outbox_full"),
                                       map(int, m.groups())))


def light_command(mode):
    return {"idle": "LIGHT off", "white": "LIGHT 100", "alt": "LIGHT alt"}[mode]


def run_cell(r1, gw, mode, runs, listen_s, name):
    cell = {"light": mode, "selftests": [], "listen": {}}
    cell["ping_before"] = ping_counters(gw, name)
    r1.cmd("PARAM SET behavior.call_enabled 0", until="status=")
    r1.cmd(light_command(mode), until="LIGHT")
    for i in range(runs):
        text = r1.cmd("SELFTEST", until="SELFTEST_SUMMARY", timeout=45)
        row = selftest_row(text)
        row["run"] = i + 1
        cell["selftests"].append(row)
        print("  %-5s %2d mic=%s amp=%s %s snr=%s-%s spread=%s floor=%s" % (
            mode, i + 1, row["mic"], row["amp"], row["reason"] or "", row["snr_min"], row["snr_max"], row["spread"],
            "/".join(str(f // 100 / 10) + "k" for f in row["floors"])), flush=True)
        r1.cmd(light_command(mode), until="LIGHT")  # a self-test leaves the strip as it was; keep it explicit
    r1.cmd("PARAM SET behavior.call_enabled 1", until="status=")
    before = status_keys(r1.cmd("RB STATUS", until="BUILD role", timeout=6))
    t0 = time.time()
    r1.drain(listen_s)
    after = status_keys(r1.cmd("RB STATUS", until="BUILD role", timeout=6))
    cell["listen"] = {"seconds": round(time.time() - t0), "before": before, "after": after,
                      "delta": {k: after[k] - before[k] for k in before if k in after}}
    cell["ping_after"] = ping_counters(gw, name)
    print("  %-5s listen %ss delta=%s" % (mode, cell["listen"]["seconds"], cell["listen"]["delta"]), flush=True)
    return cell


def summarize(cell):
    tests = cell["selftests"]
    passes = sum(1 for t in tests if t["amp"] == "PASS")
    mics = sum(1 for t in tests if t["mic"] == "PASS")
    floors = [f for t in tests for f in t["floors"]]
    tones = [x for t in tests for x in t["tones"]]
    snr = [t["snr_min"] for t in tests if t["snr_min"] is not None]
    return {"amp_pass": "%d/%d" % (passes, len(tests)), "mic_pass": "%d/%d" % (mics, len(tests)),
            "floor_median": statistics.median(floors) if floors else None, "floor_max": max(floors, default=None),
            "tone_median": statistics.median(tones) if tones else None, "snr_min": min(snr, default=None)}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--r1", required=True, help="USB port of the Node under test")
    ap.add_argument("--gw", help="USB port of the radio gateway (optional)")
    ap.add_argument("--name", default="R1", help="the Node's roster name for NET PING")
    ap.add_argument("--label", required=True, help="the firmware state and the setup, in words")
    ap.add_argument("--slug", default="run")
    ap.add_argument("--runs", type=int, default=10)
    ap.add_argument("--listen-s", type=int, default=180)
    ap.add_argument("--lights", default="idle,white,alt")
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    r1 = Port(args.r1)
    gw = Port(args.gw) if args.gw else None
    r1.drain(1.0)
    banner = r1.cmd("RB STATUS", until="BUILD role", timeout=6)
    build = re.search(r"BUILD role=\S+ git=\S+ date=.*? board=\S+", banner)
    out_dir = args.out or os.path.join("bench", "sessions", "%s-lightmatrix-%s" % (datetime.date.today(), args.slug))
    os.makedirs(out_dir, exist_ok=True)
    session = {"label": args.label, "build": build.group(0) if build else None,
               "started": datetime.datetime.now().isoformat(timespec="seconds"), "cells": []}
    print("build:", session["build"])
    try:
        for mode in args.lights.split(","):
            cell = run_cell(r1, gw, mode, args.runs, args.listen_s, args.name)
            cell["summary"] = summarize(cell)
            session["cells"].append(cell)
            print("  ->", cell["summary"], flush=True)
    finally:
        r1.cmd("LIGHT off", until="LIGHT")
        r1.cmd("PARAM SET behavior.call_enabled 1", until="status=")
        with open(os.path.join(out_dir, "lightmatrix.json"), "w") as f:
            json.dump(session, f, indent=1)
        with open(os.path.join(out_dir, "console.log"), "w") as f:
            f.write("\n".join(r1.log))
    print("saved", out_dir)


if __name__ == "__main__":
    sys.exit(main())
