#!/usr/bin/env python3
"""Run the pinned clang-format over the C/C++ sources (step 11j, .clang-format).

The file set: every tracked .h/.hpp/.c/.cpp under src/, lib/ and test/, minus the recorded captures under
test/fixtures/, generated headers (first line says "do not edit") and EXCLUDED_FILES. The version is pinned so CI
and the developer's machine format alike: install it with `python -m pip install clang-format==18.1.8`.

  python3 tools/ci/clang_format.py --check   # CI: exits 1 and names the files that drift
  python3 tools/ci/clang_format.py --fix     # rewrite them in place

The binary is $CLANG_FORMAT, else clang-format on PATH, else the one next to this Python.
"""
import os
import shutil
import subprocess
import sys

PINNED = "18.1.8"
ROOTS = ("src/", "lib/", "test/")
SUFFIXES = (".h", ".hpp", ".c", ".cpp")
EXCLUDED_DIRS = ("test/fixtures/",)
# Left as written on purpose (step 11j: the OTA code is under its own gate, step 11d).
EXCLUDED_FILES = ("src/app/FirmwareOta.cpp", "src/app/FirmwareOta.h")


def clang_format_binary():
    explicit = os.environ.get("CLANG_FORMAT")
    if explicit:
        return explicit
    found = shutil.which("clang-format")
    if found:
        return found
    scripts = os.path.dirname(sys.executable)
    for name in ("clang-format", "clang-format.exe", os.path.join("Scripts", "clang-format.exe")):
        candidate = os.path.join(scripts, name)
        if os.path.isfile(candidate):
            return candidate
    sys.exit("clang-format not found: pip install clang-format==" + PINNED)


def generated(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return "do not edit" in f.readline().lower()


def files():
    tracked = subprocess.run(["git", "ls-files", *ROOTS], capture_output=True, text=True, check=True).stdout
    out = []
    for path in tracked.splitlines():
        if not path.endswith(SUFFIXES) or path.startswith(EXCLUDED_DIRS) or path in EXCLUDED_FILES:
            continue
        if not os.path.isfile(path) or generated(path):
            continue
        out.append(path)
    return out


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ("--check", "--fix", "--list"):
        sys.exit(__doc__)
    root = subprocess.run(["git", "rev-parse", "--show-toplevel"], capture_output=True, text=True, check=True)
    os.chdir(root.stdout.strip())
    paths = files()
    if sys.argv[1] == "--list":
        print("\n".join(paths))
        return 0
    binary = clang_format_binary()
    version = subprocess.run([binary, "--version"], capture_output=True, text=True, check=True).stdout
    if PINNED not in version:
        sys.exit("clang-format " + PINNED + " expected, found: " + version.strip())
    args = ["--dry-run", "--Werror"] if sys.argv[1] == "--check" else ["-i"]
    failed = []
    for start in range(0, len(paths), 40):
        batch = paths[start:start + 40]
        result = subprocess.run([binary, "--style=file", *args, *batch], capture_output=True, text=True)
        if result.returncode != 0:
            for line in result.stderr.splitlines():
                name = line.split(":", 1)[0] if ": error:" in line else None
                if name and name not in failed:
                    failed.append(name)
            if not failed:
                failed.append(result.stderr.strip())
    if failed:
        print("\n".join(failed))
        print("clang-format: drift; run python3 tools/ci/clang_format.py --fix")
        return 1
    print("clang-format %s: %d files %s" % (PINNED, len(paths), "clean" if sys.argv[1] == "--check" else "formatted"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
