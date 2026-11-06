#!/usr/bin/env python3
"""Roadmap ID checks (docs/roadmaps/README.md, "Procedure").

An item ID (DET-015, BEH-008, ...) is cited from code, tests, decisions and the guide, so it has to stay
findable after the item is finished. The rule that keeps the active roadmaps pointing forward:

  - an active roadmap (docs/roadmaps/roadmap-*.md, not the steps file) holds no finished item: a Status
    starting with DONE, LANDED, CLOSED, PLAN ARCHIVED or REMOVED moves the whole section to
    docs/roadmaps/roadmap archive/roadmap-changelog.md;
  - every item has a Status line near its heading, in the archive too, and the archive holds finished
    items only;
  - an ID heading exists once across the active roadmaps and the archive (an ID is never reused);
  - every ID cited in code, tests, tools and docs resolves to a heading in one of the two places.

Exits 1 with one line per problem. Usage: python3 tools/ci/roadmap_ids.py [--selftest]
"""
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
ROADMAPS = "docs/roadmaps"
ARCHIVE = f"{ROADMAPS}/roadmap archive/roadmap-changelog.md"
STEPS = f"{ROADMAPS}/roadmap-0-steps.md"
FINISHED = ("DONE", "LANDED", "CLOSED", "PLAN ARCHIVED", "REMOVED")
HEADING = re.compile(r"^#{2,4} ([A-Z]{2,5}-\d{3}[a-z]?)\b")
TEXT_SUFFIXES = (".h", ".hpp", ".c", ".cpp", ".py", ".md", ".ini", ".yml", ".txt")
SCAN_DIRS = ("src", "lib", "test", "include", "tools", "docs")
STATUS_WINDOW = 8  # lines after the heading in which the Status line must stand


def files_to_scan(root):
    """Tracked text files under SCAN_DIRS and CLAUDE.md (git when available, else a walk)."""
    try:
        out = subprocess.run(["git", "ls-files"], cwd=root, capture_output=True, text=True, check=True).stdout
        names = [n for n in out.splitlines() if os.path.isfile(os.path.join(root, n))]
    except (OSError, subprocess.CalledProcessError):
        names = []
        for d in SCAN_DIRS:
            for dirpath, _, fs in os.walk(os.path.join(root, d)):
                names += [os.path.relpath(os.path.join(dirpath, f), root).replace(os.sep, "/") for f in fs]
        names.append("CLAUDE.md")
    return [n for n in names if n.endswith(TEXT_SUFFIXES) and (n.startswith(SCAN_DIRS) or n == "CLAUDE.md")]


def read_lines(root, name):
    try:
        with open(os.path.join(root, name), encoding="utf-8") as f:
            return f.read().split("\n")
    except (OSError, UnicodeDecodeError):
        return []


def items_of(root, name):
    """(id, line number, status text or None) for every item heading in a roadmap file."""
    lines = read_lines(root, name)
    items = []
    for i, line in enumerate(lines):
        m = HEADING.match(line)
        if m:
            status = next((s[7:].strip() for s in lines[i + 1 : i + 1 + STATUS_WINDOW] if s.startswith("Status:")), None)
            items.append((m.group(1), i + 1, status))
    return items


def is_finished(status):
    return status.upper().startswith(FINISHED)


def check(root):
    problems = []
    active_files = sorted(
        f"{ROADMAPS}/{n}"
        for n in os.listdir(os.path.join(root, ROADMAPS))
        if n.startswith("roadmap-") and n.endswith(".md") and f"{ROADMAPS}/{n}" != STEPS
    )
    seen = {}
    counts = [0, 0]
    for name, archived in [(f, False) for f in active_files] + [(ARCHIVE, True)]:
        for item_id, line, status in items_of(root, name):
            where = f"{name}:{line}"
            counts[archived] += 1
            if item_id in seen:
                problems.append(f"{where}: {item_id} is already an item at {seen[item_id]} (an ID is never reused)")
            seen.setdefault(item_id, where)
            if status is None:
                problems.append(f"{where}: {item_id} has no Status line within {STATUS_WINDOW} lines of its heading")
            elif not archived and is_finished(status):
                problems.append(f"{where}: {item_id} is finished ({status.split(' ')[0]}); move the section to {ARCHIVE}")
            elif archived and not is_finished(status):
                problems.append(f"{where}: {item_id} is archived but not finished (Status: {status[:40]})")
    prefixes = sorted({i.split("-")[0] for i in seen})
    cite = re.compile(r"\b(?:" + "|".join(prefixes) + r")-\d{3}[a-z]?\b")
    cited = 0
    for name in files_to_scan(root):
        if name == "tools/ci/roadmap_ids.py":
            continue
        for n, line in enumerate(read_lines(root, name), 1):
            for m in cite.finditer(line):
                cited += 1
                if m.group(0) not in seen:
                    problems.append(f"{name}:{n}: {m.group(0)} is cited but no roadmap or archive heading has it")
    return problems, counts, cited


def selftest():
    """Each rule fails on a tree built to break it, and a clean tree passes."""
    fail = []

    def tree(files):
        d = tempfile.mkdtemp()
        for rel, text in files.items():
            os.makedirs(os.path.dirname(os.path.join(d, rel)), exist_ok=True)
            with open(os.path.join(d, rel), "w", encoding="utf-8") as f:
                f.write(text)
        return d

    act = "### DET-001 - open item\n\nStatus: TODO\n\nbody\n"
    arc = "# Archive\n\n## Detection\n\n### DET-002 - done item\n\nStatus: DONE 2026-01-01\n"
    base = {f"{ROADMAPS}/roadmap-detection.md": act, ARCHIVE: arc, "src/a.cpp": "// DET-001 and DET-002\n"}
    cases = [
        ("clean tree passes", base, 0),
        ("finished item in an active roadmap", {**base, f"{ROADMAPS}/roadmap-detection.md": act.replace("TODO", "LANDED 2026")}, 1),
        ("item without Status", {**base, f"{ROADMAPS}/roadmap-detection.md": "### DET-001 - x\n\ntext\n"}, 1),
        ("unfinished item in the archive", {**base, ARCHIVE: arc.replace("DONE 2026-01-01", "PARTIAL")}, 1),
        ("ID used twice", {**base, ARCHIVE: arc.replace("DET-002", "DET-001")}, 1),
        ("cited ID without heading", {**base, "src/a.cpp": "// DET-001 DET-099\n"}, 1),
    ]
    for label, files, want in cases:
        got = len(check(tree(files))[0])
        if got < want or (want == 0 and got != 0):
            fail.append(f"selftest: {label}: expected {'no problem' if want == 0 else 'a problem'}, got {got}")
    return fail


def main():
    if "--selftest" in sys.argv[1:]:
        fail = selftest()
        print("\n".join(fail) if fail else "roadmap_ids selftest: ok")
        return 1 if fail else 0
    problems, counts, cited = check(ROOT)
    if problems:
        print("\n".join(problems))
        return 1
    print(f"roadmap_ids: ok ({counts[0]} active items, {counts[1]} archived, {cited} citations resolve)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
