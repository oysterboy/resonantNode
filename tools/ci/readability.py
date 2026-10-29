#!/usr/bin/env python3
"""Print the readability tracked set over src/ (docs/research/readability-measures.md section 3, step 12.4).

One table, A-G, plus the tail (the functions over CCN 20 or NLOC 100). Fails on nothing: the counts are read when a
pass closes, and a pass doc quotes them dated, the way bench numbers are quoted. Committed values nowhere.

  A  functions with CCN > 20
  B  functions with NLOC > 60
  C  functions with > 5 params
  D  data members in the three largest classes (`class`, not plain `struct` records; members directly in the body)
  E  headers with an owns / does-not block, among the headers that define a class or struct
  F  mean CCN, src/modes vs the rest
  G  lines over 120 columns (clang-format leaves string literals)

lizard is pinned so CI and the developer's machine count alike: `python -m pip install lizard==1.24.3`.

  python3 tools/ci/readability.py            # the table and the tail
  python3 tools/ci/readability.py --members  # also every class's member count, largest first
"""
import os
import re
import sys

PINNED = "1.24.3"
ROOT = "src"
SUFFIXES = (".h", ".hpp", ".cpp", ".c")

try:
    import lizard
except ImportError:
    sys.exit("lizard not found: pip install lizard==" + PINNED)


def sources():
    out = []
    for d, _, files in os.walk(ROOT):
        for f in files:
            if f.endswith(SUFFIXES):
                out.append(os.path.join(d, f).replace("\\", "/"))
    return sorted(out)


def strip_code(text):
    """Comments and string / char literals blanked (newlines kept), so braces and semicolons are the code's."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * text.count("\n", i, j))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            out.append(c + c)
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


CLASS_HEAD = re.compile(r"(?<!enum )\b(class|struct)\s+(\w+)\s*(?:final\s*)?(?::[^{;()]*)?\{")
NOT_MEMBER = re.compile(r"^(using|typedef|friend|static|enum|struct|class|union|template)\b")


def match_brace(code, i):
    """Index just past the brace that closes the one at i."""
    depth = 0
    while i < len(code):
        if code[i] == "{":
            depth += 1
        elif code[i] == "}":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return i


def count_members(body):
    """Data members directly in a class body (between its braces)."""
    count = 0
    stmt = ""
    i = 0
    while i < len(body):
        c = body[i]
        if c == "{":
            s = stmt.strip()
            end = match_brace(body, i)
            if "=" in s.replace("operator=", "") or ("(" not in s and not NOT_MEMBER.match(s)):
                stmt += "{}"  # a brace initializer: the statement goes on to its ';'
            else:
                stmt = ""  # a function body or a nested type
            i = end
            continue
        if c == ";":
            s = re.sub(r"\b(public|private|protected)\s*:", "", stmt).strip()
            head = s.split("=", 1)[0] if "operator" not in s else s
            if s and not NOT_MEMBER.match(s) and "(" not in head:
                count += 1
            stmt = ""
        else:
            stmt += c
        i += 1
    return count


def classes(path, code):
    for m in CLASS_HEAD.finditer(code):
        start = m.end() - 1
        body = code[start + 1 : match_brace(code, start) - 1]
        yield m.group(2), path, count_members(body), m.group(1)


OWNS = re.compile(r"(?im)^\s*(?://+|\*)?\s*(owns\b|responsibilities\b|does not\b|must not own\b)")


def main():
    files = sources()
    funcs = []
    for info in lizard.analyze_files(files):
        for f in info.function_list:
            funcs.append((info.filename.replace("\\", "/"), f))
    a = [x for x in funcs if x[1].cyclomatic_complexity > 20]
    b = [x for x in funcs if x[1].nloc > 60]
    c = [x for x in funcs if x[1].parameter_count > 5]

    all_classes = []
    headers_with_class = headers_with_owns = 0
    long_lines = 0
    for p in files:
        text = open(p, encoding="utf-8", errors="replace").read()
        long_lines += sum(1 for line in text.splitlines() if len(line) > 120)
        code = strip_code(text)
        found = list(classes(p, code))
        all_classes += found
        if p.endswith((".h", ".hpp")) and found:
            headers_with_class += 1
            headers_with_owns += 1 if OWNS.search(text) else 0
    all_classes.sort(key=lambda x: -x[2])

    def mean(xs):
        return sum(xs) / len(xs) if xs else 0.0

    modes = [f.cyclomatic_complexity for p, f in funcs if p.startswith("src/modes/")]
    rest = [f.cyclomatic_complexity for p, f in funcs if not p.startswith("src/modes/")]

    print("readability tracked set over %s/: %d files, %d functions (lizard %s)"
          % (ROOT, len(files), len(funcs), lizard.version if hasattr(lizard, "version") else PINNED))
    print("A  functions with CCN > 20                 %d" % len(a))
    print("B  functions with NLOC > 60                %d" % len(b))
    print("C  functions with > 5 params               %d" % len(c))
    print("D  members, three largest classes          %s" % " / ".join(
        "%d %s" % (n, name) for name, _, n, kind in [x for x in all_classes if x[3] == "class"][:3]))
    print("E  headers with an owns block              %d of %d" % (headers_with_owns, headers_with_class))
    print("F  mean CCN, src/modes vs the rest         %.1f / %.1f" % (mean(modes), mean(rest)))
    print("G  lines over 120                          %d" % long_lines)
    print()
    print("tail (CCN > 20 or NLOC > 100), by CCN:")
    tail = sorted({(p, f.start_line): (p, f) for p, f in a + [x for x in b if x[1].nloc > 100]}.values(),
                  key=lambda x: -x[1].cyclomatic_complexity)
    for p, f in tail:
        print("  %-58s %-36s %4d NLOC  CCN %3d  %2d params" % (
            "%s:%d" % (p[len(ROOT) + 1 :], f.start_line), f.name.split("::")[-1][:40], f.nloc, f.cyclomatic_complexity,
            f.parameter_count))
    if "--members" in sys.argv:
        print()
        print("members per class:")
        for name, p, n, kind in all_classes:
            if n:
                print("  %3d  %-6s %-36s %s" % (n, kind, name, p))
    return 0


if __name__ == "__main__":
    sys.exit(main())
