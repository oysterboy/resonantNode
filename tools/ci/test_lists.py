#!/usr/bin/env python3
"""Check that platformio.ini's twin test lists agree (review D4.3).

The ESP envs' `test_ignore` names the suites that run on the host only; the native envs' `test_filter`
lists name what the host runs. A suite added to one list and forgotten in the other either runs nowhere
or tries to build for the board. Rules:

  - host suites: the union of every native env's test_filter;
  - on-device suites (ON_DEVICE below) run on both: in a native test_filter, never in test_ignore;
  - every ESP env's test_ignore is exactly the host suites minus the on-device ones;
  - every test/test_* directory is in one of the lists.

Exits 1 with one line per mismatch. Usage: python3 tools/ci/test_lists.py [path/to/platformio.ini]
"""
import configparser
import os
import sys

ON_DEVICE = {"test_analyzer_pass_rules", "test_timebase", "test_shipped_profile"}


def load(path):
    cp = configparser.ConfigParser(interpolation=None, inline_comment_prefixes=None, strict=False)
    with open(path, encoding="utf-8") as f:
        cp.read_file(f)
    return cp


def option(cp, section, key, seen=()):
    """The env's value of key, following `extends` as PlatformIO does."""
    if section in seen or not cp.has_section(section):
        return None
    if cp.has_option(section, key):
        return cp.get(section, key)
    if cp.has_option(section, "extends"):
        for parent in cp.get(section, "extends").replace(",", " ").split():
            value = option(cp, parent.strip(), key, seen + (section,))
            if value is not None:
                return value
    return None


def names(value):
    return {n.strip() for n in (value or "").replace(",", "\n").splitlines() if n.strip()}


def main():
    root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    ini = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "platformio.ini")
    cp = load(ini)
    envs = [s for s in cp.sections() if s.startswith("env:")]
    native = [e for e in envs if (option(cp, e, "platform") or "").strip() == "native"]
    esp = [e for e in envs if e not in native]

    host = set()
    for e in native:
        host |= names(option(cp, e, "test_filter"))
    expected_ignore = host - ON_DEVICE

    errors = []
    for suite in sorted(ON_DEVICE - host):
        errors.append("%s is an on-device suite but in no native test_filter (it must run on the host too)" % suite)
    for e in esp:
        ignore = names(option(cp, e, "test_ignore"))
        for suite in sorted(ignore & ON_DEVICE):
            errors.append("[%s] test_ignore lists %s, an on-device suite" % (e, suite))
        for suite in sorted(expected_ignore - ignore):
            errors.append("[%s] test_ignore misses %s (a host suite: the board build would try it)" % (e, suite))
        for suite in sorted(ignore - ON_DEVICE - host):
            errors.append("[%s] test_ignore lists %s, which no native test_filter runs (it runs nowhere)" % (e, suite))

    test_dir = os.path.join(os.path.dirname(os.path.abspath(ini)), "test")
    if os.path.isdir(test_dir):
        for d in sorted(os.listdir(test_dir)):
            if d.startswith("test_") and os.path.isdir(os.path.join(test_dir, d)) and d not in host | ON_DEVICE:
                errors.append("test/%s is in no native test_filter and not an on-device suite" % d)

    for line in errors:
        print("test_lists: " + line)
    if errors:
        print("test_lists: FAIL (%d mismatches; fix platformio.ini's test_ignore / test_filter)" % len(errors))
        return 1
    print("test_lists: ok (%d host suites, %d on-device, %d ESP envs, %d native envs)"
          % (len(host), len(ON_DEVICE), len(esp), len(native)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
