"""Import existing run logs into a bench session (for runs not made with seqrun.py).

  python tools/bench/import_session.py --session bench/sessions/<name> \
    --purpose "..." --distance-cm 40 --firmware-log boot.log NAME=path/to/run.log ...

The firmware is read from the BUILD banner in --firmware-log (or in the run
log itself). Re-run with --reindex-only to just rebuild bench/index.csv.
"""
import argparse
import datetime
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(__file__))
import benchlib  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("--session")
ap.add_argument("--purpose")
ap.add_argument("--distance-cm", type=float)
ap.add_argument("--orientation", default="")
ap.add_argument("--emitter-fw", default="")
ap.add_argument("--note", action="append", default=[])
ap.add_argument("--firmware-log")
ap.add_argument("--reindex-only", action="store_true")
ap.add_argument("runs", nargs="*", help="NAME=path")
args = ap.parse_args()

if args.reindex_only:
    print(benchlib.rebuild_index())
    sys.exit(0)

sdir = os.path.abspath(args.session)
os.makedirs(sdir, exist_ok=True)
if os.path.exists(os.path.join(sdir, "session.json")):
    data = benchlib.load_session(sdir)
else:
    data = {"purpose": args.purpose, "created": datetime.datetime.now().isoformat(timespec="seconds"),
            "setup": {"distance_cm": args.distance_cm, "orientation": args.orientation,
                      "emitter_fw": args.emitter_fw, "notes": args.note},
            "runs": []}

fw_default = None
if args.firmware_log:
    fw_default = benchlib.parse_banner(open(args.firmware_log, encoding="utf-8", errors="replace").read())

for spec in args.runs:
    name, src = spec.split("=", 1)
    text = open(src, encoding="utf-8", errors="replace").read()
    fw = benchlib.parse_banner(text) or fw_default
    if fw is None:
        sys.exit(f"{src}: no firmware banner; pass --firmware-log")
    fname = f"{name}.{fw['git']}.log"
    started = datetime.datetime.fromtimestamp(os.path.getmtime(src)).isoformat(timespec="seconds")
    with open(os.path.join(sdir, fname), "w", encoding="utf-8", newline="\n") as f:
        f.write(f"# run: {name}\n# firmware: role={fw['role']} git={fw['git']} version={fw['version']} board={fw['board']}\n"
                f"# imported-from: {os.path.basename(src)} (mtime {started})\n")
        f.write(text.replace("\r\n", "\n"))
    data["runs"] = [r for r in data["runs"] if r["file"] != fname]
    data["runs"].append({"name": name, "file": fname, "firmware": fw, "started": started,
                         "summary": benchlib.summarize_run(os.path.join(sdir, fname))})
    print("imported", fname)

benchlib.save_session(sdir, data)
print(benchlib.rebuild_index())
