"""Compare two SEQ run logs: summary metrics, trial results, and field names.

  python tools/bench/seqcmp.py A.log B.log
  python tools/bench/seqcmp.py --session bench/sessions/<name>   # pair runs by name across firmware
  python tools/bench/seqcmp.py --table bench/sessions/<name> [name-prefix ...]   # one row per run

Field-name differences per line type are listed separately: renames are
allowed to change labels, never values (see cleanup-0-plan.md T2/T3).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import benchlib  # noqa: E402


def compare(a, b):
    pa, pb = benchlib.parse_run_log(a), benchlib.parse_run_log(b)
    sa, sb = benchlib.summarize_run(a), benchlib.summarize_run(b)
    print(f"A: {os.path.basename(a)}\nB: {os.path.basename(b)}")
    print(f"{'metric':28} {'A':>10} {'B':>10}")
    for k in benchlib.SUMMARY_FIELDS:
        va, vb = sa.get(k, "-"), sb.get(k, "-")
        mark = "" if va == vb else "  *"
        print(f"{k:28} {va:>10} {vb:>10}{mark}")
    print("trial results A:", sa["trial_results"])
    print("trial results B:", sb["trial_results"])
    print("line counts   A:", sa["line_counts"])
    print("line counts   B:", sb["line_counts"])
    for t in benchlib.LINE_TYPES:
        ka, kb = set(pa["keys"].get(t, [])), set(pb["keys"].get(t, []))
        if ka == kb:
            continue
        print(f"-- {t} fields: common={len(ka & kb)}")
        for k in sorted(ka - kb):
            print("   only A:", k)
        for k in sorted(kb - ka):
            print("   only B:", k)
    print()


TABLE = (("trials", "n"), ("expected_trials", "exp"), ("miss_trials", "miss"),
         ("rejected_trials", "rej"), ("early_trials", "early"), ("late_trials", "late"),
         ("duplicate_trials", "dup"), ("unexpected_trials", "unexp"),
         ("detector_accepted_trials", "det"), ("pattern_valid_trials", "valid"),
         ("buffer_overrun_trials", "ovr"), ("avg_strength", "str"))


def table(session_dir, prefixes):
    """One row per run in session order; '*' marks a run cut before SEQ_SUMMARY."""
    data = benchlib.load_session(session_dir)
    print(f"{'run':36} " + " ".join(f"{h:>6}" for _, h in TABLE))
    for r in data["runs"]:
        if prefixes and not any(r["name"].startswith(p) for p in prefixes):
            continue
        path = os.path.join(session_dir, r["file"])
        s = benchlib.summarize_run(path) if os.path.isfile(path) else r.get("summary", {})
        name = r["name"] + ("" if s.get("complete") else " *")
        print(f"{name:36} " + " ".join(f"{str(s.get(k, '0')):>6}" for k, _ in TABLE))


if __name__ == "__main__":
    if sys.argv[1] == "--table":
        table(sys.argv[2], sys.argv[3:])
    elif sys.argv[1] == "--session":
        data = benchlib.load_session(sys.argv[2])
        by_name = {}
        for r in data["runs"]:
            by_name.setdefault(r["name"], []).append(r["file"])
        for name, files in by_name.items():
            if len(files) >= 2:
                compare(os.path.join(sys.argv[2], files[0]), os.path.join(sys.argv[2], files[-1]))
    else:
        compare(sys.argv[1], sys.argv[2])
