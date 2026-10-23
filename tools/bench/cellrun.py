"""One NODE-012 cell (step 5h): the standard battery in one command, into one bench session.

A cell is one physical condition (space x speaker x distance x height, research/acoustic-test-suite.md).
Set it up, then run this against the Analyzer; the Emitter plays over its link as in every SEQ run.
Thresholds are not changed: a failing cell is a measurement, tuning is its own pass.

Battery (the shipped TonalPulseScalar, band 2950-3950):
  hop     SEQ, the Node's network signal (25 ms x 5, gap 10, bins 2950..3950), --tries trials
  chirp   SEQ, the plain 100 ms chirp at 3200 Hz, --tries trials
  P75     contrast P75 per run (tools/bench/inspectstats.py) against contrast_medium 30500
  RAW     --raw-reps hop captures and 3 chirp captures (tools/bench/rawcorpus.py), so later sweeps run on the host

The README gets a results table; add the room notes (what was changed, absorbers, heights) by hand.

  python tools/bench/cellrun.py --port COM3 --session bench/sessions/2026-10-24-cell-dry-110cm \
      --space "desk dry (blankets above the pair)" --distance-cm 110 --note "E3 Emitter, E2 Analyzer, both up"
"""
import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PY = sys.executable

ap = argparse.ArgumentParser()
ap.add_argument("--port", required=True, help="the Analyzer")
ap.add_argument("--session", required=True)
ap.add_argument("--space", required=True)
ap.add_argument("--distance-cm", type=float, required=True)
ap.add_argument("--speaker", default="stock D-AMP 3 W / 8 ohm")
ap.add_argument("--tone-level", default="0.5 FS")
ap.add_argument("--orientation", default="mic and speaker facing up")
ap.add_argument("--note", action="append", default=[])
ap.add_argument("--tries", type=int, default=50)
ap.add_argument("--raw-reps", type=int, default=5)
ap.add_argument("--skip-raw", action="store_true")
a = ap.parse_args()

HOP = "freq=3300 band=2950-3950 dur=25 pulses=5 gap=10 hop=2950,3200,3450,3700,3950"
runs = [
    ("hop%d" % a.tries, "SEQ start profile=TonalPulseScalar tries=%d %s mode=detail when=all verbose=1" % (a.tries, HOP)),
    ("chirp%d" % a.tries, "SEQ start profile=TonalPulseScalar tries=%d dur=100 mode=detail when=all verbose=1" % a.tries),
]
cmd = [PY, os.path.join(HERE, "seqrun.py"), "--port", a.port, "--session", a.session,
       "--purpose", "NODE-012 cell (step 5h): %s, %.0f cm" % (a.space, a.distance_cm),
       "--distance-cm", str(a.distance_cm), "--orientation", a.orientation,
       "--space", a.space, "--speaker", a.speaker, "--tone-level", a.tone_level]
for n in a.note:
    cmd += ["--note", n]
for name, command in runs:
    cmd += ["--run", name, command]
print("cell: SEQ battery", flush=True)
subprocess.run(cmd, check=True)

logs = sorted(f for f in os.listdir(a.session) if f.endswith(".log") and f.split(".")[0] in [r[0] for r in runs])
stats = subprocess.run([PY, os.path.join(HERE, "inspectstats.py")] + [os.path.join(a.session, f) for f in logs] +
                       ["--contrast-threshold", "30500"], capture_output=True, text=True).stdout
with open(os.path.join(a.session, "inspectstats.txt"), "w", encoding="utf-8") as f:
    f.write(stats)
print(stats, flush=True)

if not a.skip_raw:
    print("cell: RAW captures", flush=True)
    base = [PY, os.path.join(HERE, "rawcorpus.py"), "--port", a.port, "--out", a.session,
            "--distance-cm", str(a.distance_cm), "--orientation", a.orientation, "--band", "2950-3950",
            "--note", "NODE-012 cell: %s" % a.space]
    subprocess.run(base + ["--label", "hop5_cell", "--freq", "3300", "--dur", "25", "--pulses", "5", "--gap", "10",
                           "--hop", "2950,3200,3450,3700,3950", "--reps", str(a.raw_reps)], check=True)
    subprocess.run(base + ["--label", "chirp3200_cell", "--reps", "3"], check=True)

sys.path.insert(0, HERE)
import benchlib  # noqa: E402

data = benchlib.load_session(os.path.abspath(a.session))
rows = []
for run in data.get("runs", []):
    s = run.get("summary", {})
    rows.append("| %s | %s/%s | %s | %s | %s | %s |" % (
        run["name"], s.get("expected_trials", "?"), s.get("trials", "?"), s.get("rejected_trials", "?"),
        s.get("miss_trials", "?"), s.get("buffer_overrun_trials", "?"), s.get("avg_strength", "?")))
readme = os.path.join(a.session, "README.md")
if not os.path.exists(readme):
    with open(readme, "w", encoding="utf-8") as f:
        f.write("# NODE-012 cell: %s, %.0f cm\n\n" % (a.space, a.distance_cm))
        f.write("Step 5h. tools/bench/cellrun.py; speaker %s, tone level %s, %s.\n" % (a.speaker, a.tone_level, a.orientation))
        for n in a.note:
            f.write("- %s\n" % n)
        f.write("\n| run | expected/trials | rejected | missed | buffer overruns | avg strength |\n|---|---|---|---|---|---|\n")
        f.write("\n".join(rows) + "\n\n")
        f.write("Contrast P75 (limit 30500), inspectstats.txt:\n\n```text\n%s```\n\n" % stats)
        f.write("Room notes: (add what was changed, absorbers, heights, anything audible)\n")
print("cell: done, %s" % readme)
