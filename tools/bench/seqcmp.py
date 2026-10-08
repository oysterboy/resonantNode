"""Compare two SEQ run logs: summary metrics, trial results, and field names.

  python tools/bench/seqcmp.py A.log B.log
  python tools/bench/seqcmp.py --session bench/sessions/<name>   # pair runs by name across firmware

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


if __name__ == "__main__":
    if sys.argv[1] == "--session":
        data = benchlib.load_session(sys.argv[2])
        by_name = {}
        for r in data["runs"]:
            by_name.setdefault(r["name"], []).append(r["file"])
        for name, files in by_name.items():
            if len(files) >= 2:
                compare(os.path.join(sys.argv[2], files[0]), os.path.join(sys.argv[2], files[-1]))
    else:
        compare(sys.argv[1], sys.argv[2])
