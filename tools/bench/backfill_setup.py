"""Fill empty setup fields (space, speaker, tone_level) in old session.json files, with provenance.

  python tools/bench/backfill_setup.py            # dry run: what would be filled, and from what
  python tools/bench/backfill_setup.py --write    # write session.json files and rebuild the index

Only fills a field that is empty, and only from evidence the session already holds:

  tone_level  a single level stated in the session's own name, purpose or notes ("0p5", "0.5 FS").
              The name wins over the purpose ("-up-70cm-0p3" ran at 0.3 even if the purpose names 0.5).
              Two different levels in the text, or none: left empty. Never from firmware defaults.
              Also left empty when the level varied inside the session: the purpose names level as a
              variable, or run names carry level tags (x05, a030, 0p3).
  speaker     from the board in the runs' boot banners: D-AMP -> the stock driver, piezo -> "piezo".
  space       "desk": every bench session up to 2026-10-28 ran on the one desk
              (docs/research/acoustic-test-suite.md, "Problem").

Each filled field is recorded in setup["backfill"] as {field: source}, and the index's
`backfilled` column names those fields, so a backfilled value is never mistaken for one
recorded at the bench. Added 2026-10-28 for docs/research/distance-range.md.
"""
import argparse
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(__file__))
import benchlib  # noqa: E402

VARIED = re.compile(r"(?:^|_)(?:x0\d|a0\d\d|0p\d)(?:_|$)")
LEVEL = re.compile(r"\b0[.,]([1-9])\s*FS\b|(?:^|[-_ ])0p([1-9])(?=$|[-_ .])", re.I)
DESK_UNTIL = "2026-10-28"


def levels(text):
    return {"0.%s FS" % (a or b) for a, b in LEVEL.findall(text)}


def propose(name, data):
    setup = data.get("setup", {})
    out = {}
    varied = (re.search(r"\blevels?\b", data.get("purpose") or "", re.I)
              or any(VARIED.search(r.get("name", "")) for r in data.get("runs", [])))
    if not setup.get("tone_level") and not varied:
        in_name = levels(name)
        in_text = levels(" ".join([data.get("purpose") or ""] + list(setup.get("notes") or [])))
        if len(in_name) == 1:
            out["tone_level"] = (in_name.pop(), "session name")
        elif not in_name and len(in_text) == 1:
            out["tone_level"] = (in_text.pop(), "purpose/notes")
    if not setup.get("speaker"):
        boards = {r.get("firmware", {}).get("board", "") for r in data.get("runs", [])} - {""}
        if boards == {"damp"}:
            out["speaker"] = (benchlib.DEFAULT_SPEAKER, "boot banner board=damp")
        elif boards == {"piezo"}:
            out["speaker"] = ("piezo", "boot banner board=piezo")
    if not setup.get("space") and name[:10] <= DESK_UNTIL:
        out["space"] = ("desk", "all bench sessions to %s ran on the one desk" % DESK_UNTIL)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    root = os.path.join(benchlib.BENCH_ROOT, "sessions")
    counts, left = {}, []
    for sj in sorted(glob.glob(os.path.join(root, "*", "session.json"))):
        sdir = os.path.dirname(sj)
        name = os.path.basename(sdir)
        data = benchlib.load_session(sdir)
        fills = propose(name, data)
        setup = data.setdefault("setup", {})
        for field, (value, source) in fills.items():
            counts[field] = counts.get(field, 0) + 1
            print("%-48s %-10s = %-24s (%s)" % (name, field, value, source))
            setup[field] = value
            setup.setdefault("backfill", {})[field] = source
        if not setup.get("tone_level"):
            left.append(name)
        if fills and args.write:
            benchlib.save_session(sdir, data)
    print("\nfilled:", counts)
    print("tone_level still empty in %d sessions (no single level stated): %s" % (len(left), ", ".join(left)))
    if args.write:
        print(benchlib.rebuild_index())
    else:
        print("dry run; --write to apply")


if __name__ == "__main__":
    main()
