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
  - every ID cited in code, tests, tools and docs resolves to a heading in one of the two places;
  - a finished step in the steps file is folded to its header, a Result line and a "Full text:" pointer to the
    phase archive in roadmap archive/ (that file must exist): its plan and progress lines live there.

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
STEP = re.compile(r"^(\d+)\. \[([A-Z]+)")
DONE_STEP_MAX_LINES = 6  # header, a Result line wrapped once or twice, the pointer
FULL_TEXT = re.compile(r"Full text: (roadmap archive/[\w.-]+\.md)")


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


def check_steps(root):
    """A DONE step in the steps file's Order section is a short stub pointing at its full text."""
    lines = read_lines(root, STEPS)
    problems = []
    if "## Order" not in lines:
        return problems
    start = lines.index("## Order") + 1
    end = next((i for i in range(start, len(lines)) if lines[i].startswith("## ")), len(lines))
    heads = [i for i in range(start, end) if STEP.match(lines[i])]
    for k, i in enumerate(heads):
        block = [l for l in lines[i : heads[k + 1] if k + 1 < len(heads) else end] if l.strip()]
        number, status = STEP.match(lines[i]).groups()
        if status != "DONE":
            continue
        where = f"{STEPS}:{i + 1}"
        pointer = next((FULL_TEXT.search(l) for l in block if FULL_TEXT.search(l)), None)
        if len(block) > DONE_STEP_MAX_LINES:
            problems.append(f"{where}: step {number} is DONE with {len(block)} lines; move its text to the phase archive "
                            f"in {ROADMAPS}/roadmap archive/ and keep the header, a Result line and a Full text pointer")
        elif pointer is None:
            problems.append(f"{where}: step {number} is DONE without a 'Full text: roadmap archive/<file>.md' pointer")
        elif not os.path.isfile(os.path.join(root, ROADMAPS, pointer.group(1))):
            problems.append(f"{where}: step {number} points at {pointer.group(1)}, which does not exist")
    return problems


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
    problems += check_steps(root)
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
    PHASE = f"{ROADMAPS}/roadmap archive/roadmap-0-steps-9.md"
    steps = "# Steps\n\n## Order\n\n{done}\n2. [TODO] next (DET-001)\n   plan\n\n## Notes\n"
    stub = "1. [DONE 2026-01-01] first (DET-002)\n   Result: done.\n   Full text: roadmap archive/roadmap-0-steps-9.md, step 1.\n"
    long_text = "".join(f"   line {n}\n" for n in range(6))
    cases = [
        ("clean tree passes", base, 0),
        ("finished item in an active roadmap", {**base, f"{ROADMAPS}/roadmap-detection.md": act.replace("TODO", "LANDED 2026")}, 1),
        ("item without Status", {**base, f"{ROADMAPS}/roadmap-detection.md": "### DET-001 - x\n\ntext\n"}, 1),
        ("unfinished item in the archive", {**base, ARCHIVE: arc.replace("DONE 2026-01-01", "PARTIAL")}, 1),
        ("ID used twice", {**base, ARCHIVE: arc.replace("DET-002", "DET-001")}, 1),
        ("cited ID without heading", {**base, "src/a.cpp": "// DET-001 DET-099\n"}, 1),
        ("finished step folded", {**base, STEPS: steps.format(done=stub), PHASE: "# phase\n"}, 0),
        ("finished step left long", {**base, STEPS: steps.format(done=stub + long_text), PHASE: "# phase\n"}, 1),
        ("finished step without pointer", {**base, STEPS: steps.format(done="1. [DONE 2026-01-01] x\n   Result: y\n")}, 1),
        ("pointer to a missing archive", {**base, STEPS: steps.format(done=stub)}, 1),
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
