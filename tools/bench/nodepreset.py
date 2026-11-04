"""Named Node settings for listening sessions, applied live (PARAM SET, not saved) to every board in one go.

  python tools/bench/nodepreset.py --boards E1=COM8,E2=COM3,... <preset>
  python tools/bench/nodepreset.py --list

The ports open without resetting the boards and close again (a closing port does not reset these D-AMP boards when
DTR / RTS stay low). The boards keep the values until a reboot or PARAM LOAD; "quiet" silences them.
"""
import argparse
import time

import serial

COMMON = ["PARAM SET behavior.band_per_node 1", "PARAM SET behavior.base_scatter_bins 1", "PARAM SET output.silent 0"]
PRESETS = {
    "wide": ("the 2026-10-31 field: 950-4950 Hz, the defaults' pace",
             COMMON + ["PARAM SET behavior.band_range_low_hz 950", "PARAM SET behavior.band_range_high_hz 4950",
                       "PARAM SET behavior.level_max 6", "PARAM SET behavior.level_decay_ms 2000",
                       "PARAM SET behavior.answer_jitter_ms 3000", "PARAM SET output.idle_announce_hz 300",
                       "PARAM SET output.idle_announce_ms 500"]),
    "low": ("the 41 h run: 300-5000 Hz (500 then), lively and quick, a short 120 Hz idle tone",
            COMMON + ["PARAM SET behavior.band_range_low_hz 300", "PARAM SET behavior.band_range_high_hz 5000",
                      "PARAM SET behavior.level_max 4", "PARAM SET behavior.level_decay_ms 5000",
                      "PARAM SET behavior.answer_jitter_ms 800", "PARAM SET output.idle_announce_hz 120",
                      "PARAM SET output.idle_announce_ms 80"]),
    "deep": ("only the low octaves: 300-2000 Hz (three positions), lively and quick, the short idle tone",
             COMMON + ["PARAM SET behavior.band_range_low_hz 300", "PARAM SET behavior.band_range_high_hz 2000",
                       "PARAM SET behavior.level_max 4", "PARAM SET behavior.level_decay_ms 5000",
                       "PARAM SET behavior.answer_jitter_ms 800", "PARAM SET output.idle_announce_hz 120",
                       "PARAM SET output.idle_announce_ms 80"]),
    "calm": ("300-5000 Hz, slow and sparse: answers rarer and later",
             COMMON + ["PARAM SET behavior.band_range_low_hz 300", "PARAM SET behavior.band_range_high_hz 5000",
                       "PARAM SET behavior.level_max 8", "PARAM SET behavior.level_decay_ms 2000",
                       "PARAM SET behavior.answer_jitter_ms 3000", "PARAM SET output.idle_announce_hz 120",
                       "PARAM SET output.idle_announce_ms 80"]),
    "quiet": ("every board silent (detection runs)", ["PARAM SET output.silent 1"]),
}

ap = argparse.ArgumentParser()
ap.add_argument("preset", nargs="?")
ap.add_argument("--boards", default="")
ap.add_argument("--list", action="store_true")
a = ap.parse_args()
if a.list or not a.preset:
    for k, (what, _) in PRESETS.items():
        print("%-6s %s" % (k, what))
    raise SystemExit
what, cmds = PRESETS[a.preset]
for spec in a.boards.split(","):
    name, port = spec.split("=")
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.1
    s.dtr = False
    s.rts = False
    s.open()
    s.reset_input_buffer()
    bad = []
    for c in cmds:
        s.write((c + "\n").encode())
        time.sleep(0.3)
        reply = s.read(8192).decode(errors="replace")
        if "status=ok" not in reply and "unchanged" not in reply:
            bad.append(c.split()[2])
    s.close()
    print("%s %s: %s" % (name, a.preset, "ok" if not bad else "check " + ", ".join(bad)))
print(what)
