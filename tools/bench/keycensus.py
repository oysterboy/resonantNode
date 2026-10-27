"""Census of the k=v keys on the Analyzer's SEQ lines across bench logs: how many lines carry
each key, how many distinct values it took, its most common value, and whether it was constant.

A key that is CONST over every committed run never carried information to the bench; a key no
tool parses (see the audit in docs/refactors/analyzer-run-trial-split.md, "Output audit") is a
candidate to drop. Stdlib only; reads whatever logs it is given.

  python tools/bench/keycensus.py bench/sessions/*/*.log
  python tools/bench/keycensus.py --types SEQ_SUMMARY,SEQ_TRIAL bench/sessions/2026-10-2*/*.log
"""
import argparse
import collections
import re

KV = re.compile(r"(\S+?)=(\S*)")
DEFAULT_TYPES = ("SEQ_SUMMARY", "SEQ_TRIAL", "SEQ_EXPLAIN", "SEQ_INSPECT", "SEQ_SOURCE_CORE",
                 "SEQ_SOURCE_SPEC", "SEQ_SOURCE", "SEQ_HISTDBG", "SEQ_TIMING")


def census(paths, types):
    stats = {t: {} for t in types}
    lines = collections.Counter()
    sessions = collections.defaultdict(set)
    for path in paths:
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                line = line.strip()
                t = next((t for t in types if line.startswith(t + " ")), None)
                if not t:
                    continue
                lines[t] += 1
                sessions[t].add(path)
                for k, v in KV.findall(line):
                    stats[t].setdefault(k, collections.Counter())[v] += 1
    return stats, lines, sessions


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--types", default=",".join(DEFAULT_TYPES), help="comma-separated line prefixes")
    a = ap.parse_args()
    types = tuple(t for t in a.types.split(",") if t)
    stats, lines, sessions = census(a.logs, types)
    for t in types:
        print("\n== %s: %d lines in %d files" % (t, lines[t], len(sessions[t])))
        for k, c in sorted(stats[t].items(), key=lambda kv: -sum(kv[1].values())):
            n = sum(c.values())
            top, top_n = c.most_common(1)[0]
            flag = "CONST" if len(c) == 1 else ("~const(%d%% %s)" % (100 * top_n // n, top) if top_n > 0.97 * n else "")
            print("  %-40s n=%-6d distinct=%-5d top=%-18s %s" % (k, n, len(c), top[:18], flag))


if __name__ == "__main__":
    main()
