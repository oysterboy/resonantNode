#!/usr/bin/env python3
"""Include-direction checks (NODE-007, PAR-016).

1. Nothing under src/detection/ may include src/modes/. Strict in CI since ANA-003 (step 10) moved the
   AnalyzerApp member files to src/modes/analyzer/.
2. Nothing under lib/ParamRegistry/ may include anything from src/ (always strict: the library must stay
   app-agnostic so a later split into its own repo is a move, PAR-016/PAR-017).

The spec's layering (docs/specs/myspec.md) has detection below the modes. Today the Analyzer files under
src/detection/analyzer/ still include modes/analyzer/AnalyzerModeApp.h (ANA-003, step 10 moves them out), so
the check warns by default and fails only with --strict. After ANA-003 lands, CI switches to --strict.

  python tools/ci/include_direction.py [--strict]
"""
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
MODES = os.path.join(ROOT, "src", "modes") + os.sep
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')


def main():
    strict = "--strict" in sys.argv[1:]
    base = os.path.join(ROOT, "src", "detection")
    hits = []
    for dirpath, _, files in os.walk(base):
        for f in files:
            if not f.endswith((".h", ".hpp", ".cpp", ".c")):
                continue
            path = os.path.join(dirpath, f)
            for n, line in enumerate(open(path, encoding="utf-8", errors="replace"), 1):
                m = INCLUDE.match(line)
                if not m:
                    continue
                target = os.path.normpath(os.path.join(dirpath, m.group(1)))
                if (target + os.sep).startswith(MODES):
                    hits.append("%s:%d: includes %s" % (os.path.relpath(path, ROOT), n, m.group(1)))
    for h in hits:
        print(("error: " if strict else "warning: ") + h)
    print("include direction: %d include(s) of src/modes/ from src/detection/ (%s)" % (
        len(hits), "strict" if strict else "warning only until ANA-003"))

    lib_hits = []
    src_root = os.path.join(ROOT, "src") + os.sep
    lib_base = os.path.join(ROOT, "lib", "ParamRegistry")
    for dirpath, _, files in os.walk(lib_base):
        for f in files:
            if not f.endswith((".h", ".hpp", ".cpp", ".c")):
                continue
            path = os.path.join(dirpath, f)
            for n, line in enumerate(open(path, encoding="utf-8", errors="replace"), 1):
                m = INCLUDE.match(line)
                if not m:
                    continue
                target = os.path.normpath(os.path.join(dirpath, m.group(1)))
                if (target + os.sep).startswith(src_root):
                    lib_hits.append("%s:%d: includes %s" % (os.path.relpath(path, ROOT), n, m.group(1)))
    for h in lib_hits:
        print("error: " + h)
    print("include direction: %d include(s) of src/ from lib/ParamRegistry/ (strict)" % len(lib_hits))
    return 1 if (strict and hits) or lib_hits else 0


if __name__ == "__main__":
    sys.exit(main())
