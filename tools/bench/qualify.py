"""Qualify one D-AMP board for the five-node set (step 5c, issue #34).

Flashes the Node build onto PORT, then runs SELFTEST on it with a reference
board as helper (external chirp check both ways) and files the result as a bench
session named qualify-<label>. One command per board:

  python tools/bench/qualify.py --port COM5 --label E4 --reference COM4

The reference board must run the Node build (flash it first if it is an Emitter).
A board whose MAC is not in the table in bench/README.md shows up as unknown:
add its MAC there so later sessions label it. Never use this for piezo boards.
"""
import argparse
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PIO = os.path.expanduser("~/.platformio/penv/Scripts/pio.exe")
PY = sys.executable

ap = argparse.ArgumentParser()
ap.add_argument("--port", required=True, help="the new board")
ap.add_argument("--label", required=True, help="E4, E5, ...")
ap.add_argument("--reference", required=True, help="port of a working Node board (helper for external_chirp)")
ap.add_argument("--no-flash", action="store_true", help="board already runs the Node build")
ap.add_argument("--distance-cm", type=float, default=90.0)
a = ap.parse_args()

if not a.no_flash:
    print(f"flashing esp32dev (Node, D-AMP) onto {a.port} ...")
    r = subprocess.run([PIO, "run", "-e", "esp32dev", "-t", "upload", "--upload-port", a.port], cwd=ROOT)
    if r.returncode != 0:
        sys.exit("flash failed: if the board needs download mode, hold BOOT, tap EN, release, then retry")

cmd = [PY, "-I", os.path.join(ROOT, "tools", "bench", "selftest.py"),
       "--ports", a.port, a.reference, "--reset", "--external",
       "--slug", f"qualify-{a.label}", "--distance-cm", str(a.distance_cm),
       "--note", f"qualification of {a.label} for the five-node set (step 5c, #34)"]
sys.exit(subprocess.run(cmd, cwd=ROOT).returncode)
