# Step 12.4: the readability pass (docs/research/readability-measures.md, all of it)

Status: active (opened 2026-10-29). Step 12.4 (roadmap-0-steps.md, issue #83). Owner 2026-10-29: "12.4. Address the
whole readability findings." Integrates the other session's "12.5" (branch ccr-3cb2449c-sp2ed1, not merged; its issue
#83 is this step's issue).

## Goal

The code reads easier where the counts and the human read agree it is hard: state (the detector's 68 members,
Behavior's 46), the two Analyzer giants, the long reporters, the duplicated report structs, bare citations, missing
owns blocks. No behaviour and no output change anywhere.

Defaults for the note's open choices (owner may overrule): the reporters get a `LineWriter` (key=value pairs, same
bytes), not a table-driven format; citations: only the 11 bare ones are rewritten, plus a CLAUDE.md rule that new
comments state the reason and point to a decision file, not a step number; owns / does-not blocks for classes, not for
plain data structs.

## Items, in order (one commit each)

1. **12.4.1 the tracked set.** `tools/ci/readability.py`: the note's table A-G (functions over CCN 20, over 60 NLOC,
   over 5 params; members of the three largest classes; headers with an owns block; mean CCN modes vs the rest; lines
   over 120). lizard pinned like clang-format; printed in CI, never failing. The "before" goes into Progress, dated.
2. **12.4.2 detector state.** `ScalarTransientDetector`'s 68 members into the groups its comments name (live gate,
   candidate lifecycle, occurrence emission, detector report, reject summary), each reset by `= {}`.
3. **12.4.3 Behavior state.** `ResonantBehavior`'s 46 members: timing deadlines, counters, pending request, rest.
4. **12.4.4 the report builder.** `buildSequenceAnalyzerReport` (301 NLOC, 10 parameters) into one function per
   report section.
5. **12.4.5 RAW capture.** `runRawTrigger` (620 NLOC, CCN 171) into setup / capture / dump stages with one RAII buffer
   in place of the manual malloc / free pairs.
6. **12.4.6 the reporters.** printSystemHealth, printRbStatus, printDetectorReportGenericLine, printAudioRunSummary,
   printSequenceInspectCanonical through a `LineWriter`.
7. **12.4.7 the report twins.** `AcceptedOccurrenceSummary` / `SelectedRejectSummary` (22 identical fields) as one
   `OccurrenceSummary` plus the reject-only fields.
8. **12.4.8 citations.** The 11 bare `(step N)` / `(issue #N)` comments state their reason; the CLAUDE.md rule.
9. **12.4.9 owns blocks** for the classes that lack one.
10. The tracked set again; the after beside the before.

## Gates

Every item: gate.sh fully green (every env, host suites, T2h byte-identical, replay Null == diag, include direction),
test_lists.py, clang_format.py --check. Printed lines byte-identical: test_report_printers pins SEQ_SOURCE_CORE /
SPEC; for 12.4.4-12.4.7 also on the boards (remote, over WiFi): tools/bench/analyzerab.py against
bench/sessions/2026-10-29-step12-sanity/ab_ba34561 (seqcmp: same keys, same results), RAW captures before / after
(12.4.5: same header, same columns, same row count), tools/bench/nodecheck.py 27 / 27 (12.4.6: RB STATUS keys).

## Don't touch

Thresholds, detection behaviour, Behavior's decisions, line formats, the NET format, FirmwareOta, the secrets header.

## Progress

- 2026-10-29: opened.
