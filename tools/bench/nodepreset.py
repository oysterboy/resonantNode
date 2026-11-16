"""Named Node setups as files (tools/bench/presets/<name>.preset), applied live to boards, or captured from one.

  python tools/bench/nodepreset.py --list
  python tools/bench/nodepreset.py <name> --boards E1=COM8,E2=COM3            # by USB
  python tools/bench/nodepreset.py <name> --gateway E0=COM12 --targets R1,E2  # by radio (NET <target> ...)
  python tools/bench/nodepreset.py --capture <name> --gateway E0=COM12 --from R1 --what "one line"
  python tools/bench/nodepreset.py --capture <name> --boards R1=COM3 --what "one line"

A preset file is plain text: '#' lines are comments (the first one is the preset's one-line description), every other
line is a Node command, usually `PARAM SET <path> <value>`. Applying sends them in order and checks each PARAM SET
reply (status=ok) per board. Nothing is saved: the boards keep the values until a reboot or PARAM LOAD, so a preset
never replaces the stored setup (send PARAM SAVE yourself if a preset should survive a reboot).

A capture asks one board for PARAM DUMP and writes every detection.*, behavior.* and output.* value it holds, except
the per-node ones (output.hop_rotation, output.silent), so the file restores the whole setup, not only a difference.

Ports open without resetting the boards (DTR / RTS low) and close again. Through a gateway, the gateway's port is the
only one opened; a Node used as the gateway runs `NET *` commands itself too.
"""
import argparse
import os
import re
import time

import serial

HERE = os.path.dirname(os.path.abspath(__file__))
PRESET_DIR = os.path.join(HERE, "presets")
CAPTURED_MODULES = ("detection.", "behavior.", "output.")
PER_NODE = {"output.hop_rotation", "output.silent"}


def preset_path(name):
    return os.path.join(PRESET_DIR, name + ".preset")


def read_preset(name):
    what, cmds = "", []
    for line in open(preset_path(name), encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        if line.startswith("#"):
            if not what:
                what = line.lstrip("# ")
            continue
        cmds.append(line)
    return what, cmds


def open_port(port, timeout=0.1):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, timeout
    s.dtr = False
    s.rts = False
    s.open()
    s.reset_input_buffer()
    return s


def parse(spec):
    return [tuple(x.split("=")) for x in spec.split(",") if x.strip()]


def read_lines(s, secs, until=None):
    """Lines for up to secs seconds, or until until(line) is true."""
    out, buf, end = [], b"", time.time() + secs
    while time.time() < end:
        chunk = s.read(4096)
        if not chunk:
            continue
        buf += chunk
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            text = line.decode("ascii", errors="replace").strip()
            out.append(text)
            if until and until(text):
                return out
    return out


def path_of(cmd):
    parts = cmd.split()
    return parts[2] if len(parts) >= 4 and parts[0] == "PARAM" and parts[1] == "SET" else None


def set_ok(lines, path):
    return any(("path=%s " % path) in l and ("status=ok" in l or "unchanged" in l) for l in lines)


def apply_usb(boards, cmds):
    for name, port in boards:
        s = open_port(port)
        bad = []
        for c in cmds:
            s.write((c + "\n").encode())
            lines = read_lines(s, 0.4)
            p = path_of(c)
            if p and not set_ok(lines, p):
                bad.append(p)
        s.close()
        print("%s: %s" % (name, "ok" if not bad else "check " + ", ".join(bad)))


def apply_radio(gateway, targets, cmds):
    s = open_port(gateway[1])
    bad = {t: [] for t in targets}
    for c in cmds:
        p = path_of(c)
        for t in targets:
            s.write(("NET %s %s\n" % (t, c)).encode())
            lines = read_lines(s, 1.5, until=lambda l, t=t: l.startswith("NET reply from=%s " % t) and " done " in l)
            mine = [l for l in lines if l.startswith("NET reply from=%s " % t)]
            if p and not set_ok(mine, p):
                bad[t].append(p)
    s.close()
    for t in targets:
        print("%s: %s" % (t, "ok" if not bad[t] else "check " + ", ".join(bad[t])))


VALUE = re.compile(r"PARAM path=(\S+) module=\S+ type=(\S+) value=(\S+)")


def capture(args):
    if args.gateway:
        gw = parse(args.gateway)[0]
        s = open_port(gw[1])
        tag = "NET reply from=%s " % args.from_
        # A radio reply comes in parts and a part can be lost: only a dump with every part counts (3 tries).
        for attempt in range(3):
            s.write(("NET %s PARAM DUMP\n" % args.from_).encode())
            lines = read_lines(s, 30, until=lambda l: l.startswith(tag) and " done " in l)
            lines = [l for l in lines if l.startswith(tag)]
            total = next((int(m.group(1)) for m in (re.search(r"done parts=(\d+)", l) for l in lines) if m), None)
            seen = {int(m.group(1)) for m in (re.search(r" part=(\d+) ", l) for l in lines) if m}
            missing = sorted(set(range(total)) - seen) if total is not None else ["the done line"]
            if not missing:
                break
            print("attempt %d: missing parts %s" % (attempt + 1, missing))
            time.sleep(1.0)
        else:
            s.close()
            raise SystemExit("no complete PARAM DUMP from %s in 3 tries" % args.from_)
        source = "%s by radio through %s" % (args.from_, gw[0])
    else:
        name, port = parse(args.boards)[0]
        s = open_port(port)
        s.write(b"PARAM DUMP\n")
        lines = read_lines(s, 8)
        source = "%s on %s" % (name, port)
    s.close()
    values = []
    for l in lines:
        m = VALUE.search(l)
        if not m:
            continue
        path, typ, value = m.groups()
        if not path.startswith(CAPTURED_MODULES) or path in PER_NODE:
            continue
        if typ == "float":
            value = ("%f" % float(value)).rstrip("0").rstrip(".")
        values.append((path, value))
    if not values:
        raise SystemExit("no PARAM lines captured (%d lines read)" % len(lines))
    os.makedirs(PRESET_DIR, exist_ok=True)
    dest = preset_path(args.capture)
    if os.path.exists(dest) and not args.force:
        raise SystemExit("%s exists; --force to overwrite" % dest)
    with open(dest, "w", encoding="utf-8", newline="\n") as f:
        f.write("# %s\n" % (args.what or args.capture))
        f.write("# captured %s from %s, %d params (detection, behavior, output; not rotation or silent)\n"
                % (time.strftime("%Y-%m-%d %H:%M"), source, len(values)))
        for path, value in values:
            f.write("PARAM SET %s %s\n" % (path, value))
    print("%s: %d params" % (dest, len(values)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("preset", nargs="?")
    ap.add_argument("--boards", default="", help="NAME=PORT,... by USB")
    ap.add_argument("--gateway", default="", help="NAME=PORT of the radio gateway")
    ap.add_argument("--targets", default="", help="Nodes to reach through the gateway, e.g. R1,E2 (named, so a silent one shows)")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--capture", default="", help="write this preset from one board's PARAM DUMP")
    ap.add_argument("--from", dest="from_", default="", help="the board to capture from, through --gateway")
    ap.add_argument("--what", default="", help="the captured preset's one-line description")
    ap.add_argument("--force", action="store_true")
    a = ap.parse_args()
    if a.capture:
        capture(a)
        return
    if a.list or not a.preset:
        for f in sorted(os.listdir(PRESET_DIR)):
            if f.endswith(".preset"):
                what, cmds = read_preset(f[:-7])
                print("%-10s %3d lines  %s" % (f[:-7], len(cmds), what))
        return
    what, cmds = read_preset(a.preset)
    if a.gateway:
        if not a.targets:
            raise SystemExit("--targets R1,E2,... is needed with --gateway")
        apply_radio(parse(a.gateway)[0], [t for t in a.targets.split(",") if t], cmds)
    else:
        apply_usb(parse(a.boards), cmds)
    print(what)


if __name__ == "__main__":
    main()
