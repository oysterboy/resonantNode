# Readability measures: what to count, what the counts say, what to do

```text
Status:  promoted (2026-10-29)
Became:  step 12.5 (roadmap-0-steps.md, issue #83, 2026-10-29): sections 3
         and 4 in the order of section 5
Lab:     none; a code reading at main 6b9d3d0 and counts re-run at main
         26475ce7 (after step 12's review fixes), from a cloud container
         (no build, no hardware). Counts: lizard 1.24.3 over src/
         (cyclomatic complexity, NLOC, parameters), python for the rest.
         Every file:line below was read before it was written down; line
         numbers are as of 26475ce7.
```

The two earlier readings (`architecture-readability-review-2026-10-25.md`,
`code-standards-read-2026-10-28.md`) are judgement notes: a reader went
through the tree and wrote down what tripped them. This note asks the
other question: which readability properties of this tree can be
*counted*, what the counts are today, how far they agree with the human
read, and which of them are worth tracking so that the next cleanup pass
and the next three months of growth can be judged by the same yardstick.
It does not repeat the earlier notes' findings; where a count lands on one
of their items the item is named.

Two uses of "measure" are kept apart on purpose: section 2 is the
measurements, section 4 is the measures to take.

Calibration first: the tree reads well. A C++ reader with some signal
background follows the detection and behavior layers from the headers
down. The mode layer is where every count below gets worse, and the
counts put the whole of the remaining difficulty into a short list of
functions and two classes rather than into the tree as a whole.

## 1. What a human trips over, in one page

Read before counting, so the counts could be checked against it:

- `ScalarTransientDetector.cpp`, `ResonantBehavior.cpp`,
  `DetectionRuntime.h`, `VerdictProjection.h`, `DetectorReport.h` in
  full; `AnalyzerModeApp.{h,cpp}` in large slices.
- What helps: every major class opens with an owns / responsibilities /
  does-not block; names carry units (`nowMs`, `durationMs`, `lowHz` are
  the three most common identifiers in headers); the detector's
  `update()` is a 20-line checklist calling five named stages; the
  acceptance rule in `updateTransientStage` is six named booleans and one
  `accepted`; comments explain why (the wrap-safe deadline note at
  `ResonantBehavior.cpp:357-360` is the model).
- What costs: state. `ScalarTransientDetector` carries 68 members,
  `ResonantBehavior` 46; a function's logic is simple but which fields it
  reads and writes is not visible from its signature. `AnalyzerApp`'s
  methods are spread over eleven `.cpp` files. Comments cite `step 8b
  item 3`, `issue #31 R2-14`, `DET-015` 217 times inside bodies: perfect
  breadcrumbs for the owner, noise for a newcomer until they know the
  roadmap vocabulary. `AcceptedOccurrenceSummary` and
  `SelectedRejectSummary` (`DetectorReport.h:14-70`) are the same 22
  fields twice with a "compatibility alias for peak in this pass" that
  marks a migration left half done.

## 2. The measurements at 26475ce7

### 2.1 Size and shape

```text
src/                      149 files   24,144 lines   898 functions
file size                 78 files < 100 lines, 52 of 100-300, 11 of
                          300-600, 8 over 600 (largest AnalyzerModeApp.cpp
                          1131)
function size (NLOC)      median 7; 104 over 30, 46 over 60, 18 over 100
cyclomatic (CCN)          median 2, mean 4.6; 79 over 10, 34 over 20,
                          4 over 50
parameters                367 functions take none, 291 one; 13 take six
                          or more (rawHealthClassNameFromCounters with 16,
                          buildSequenceAnalyzerReport with 10,
                          magnitudeModule with 9, runRawTrigger and the
                          self-test checks with 7)
brace nesting             max 7 (AudioSelfTest.cpp); 6 in I2sToneOutput,
                          AnalyzerRawCapture, Replay, FeatureHistory,
                          NetFrame.h
line length               12 lines over 120 (clang-format leaves string
                          literals); the longest is 299 columns,
                          AnalyzerModeApp.cpp:586, one printf format
comment lines             1,835 of 24,144 (7 %); 21 of the 97 headers
                          (61 declare a class or struct) open with an
                          owns / does-not block
ternaries                 572; 16 lines carry two or more
include fan-in            ResonantNodeApp.h 26 includes,
                          AnalyzerModeApp.cpp 23, Replay.cpp 21
```

Per directory (lizard; `n` functions, mean NLOC, mean and max CCN, how
many functions exceed CCN 10 and NLOC 60):

```text
dir              n    NLOC   CCN   maxCCN  CCN>10  NLOC>60
src/audio       36     5.6   1.5      4       0       0
src/behavior    72     8.3   2.4     21       3       2
src/app         84     8.6   2.7     23       2       2
src/output      42     9.2   3.0     13       2       0
src/hal         29    11.0   3.1     30       1       1
src/detection  262    12.7   4.1     40      16       8
src/net         24    12.7   4.5     13       1       0
src/selftest    26    19.9   6.0     31       4       3
src/modes      297    24.0   6.3    171      45      26
src/host        26    25.3   8.4     45       5       4
```

The split the human read reported is in the numbers: everything outside
`src/modes/` and `src/host/` has a mean CCN under 4.5 and at most eight
functions over NLOC 60; `src/modes/` alone holds 45 of the 79 complex
functions and 26 of the 46 long ones, with a mean function three times
the size of `src/behavior`'s. (`src/host/` is the replay command line:
three functions, a CSV loader, a main and the replay loop, carry its
mean.)

### 2.2 The tail

The four functions over CCN 50 and the long ones that go with them:

```text
AnalyzerRawCapture.cpp:105     AnalyzerApp::runRawTrigger            620 NLOC  CCN 171  7 params
AnalyzerModeApp.cpp:816        buildSequenceAnalyzerReport           301 NLOC  CCN 103  10 params
AnalyzerCommands.cpp:491       applySeqStartToken                    156 NLOC  CCN  58
AnalyzerTrialCapture.cpp:17    handleSequencePending                 135 NLOC  CCN  54
Replay.cpp:124                 hostreplay::loadRawCsv                129 NLOC  CCN  45
ReplayMain.cpp:58              main                                  145 NLOC  CCN  43
Replay.cpp:453                 hostreplay::replay                    138 NLOC  CCN  42
AnalyzerModeApp.cpp:628        AnalyzerApp::update                   125 NLOC  CCN  39
AnalyzerSeqReporter.cpp:168    printSequenceInspectCanonical          97 NLOC  CCN  39
FeatureHistory.cpp:267         FeatureHistory::getWindow             149 NLOC  CCN  38
EmitterApp.cpp:308             handlePulsesCommand                   112 NLOC  CCN  37
AnalyzerSystemReporter.cpp:13  printSystemHealth                     197 NLOC  CCN  33
AnalyzerSequenceSession.cpp:149 startSequenceTest                    175 NLOC  CCN  27
ResonantNodeStatus.cpp:48      Node::printRbStatus                   151 NLOC  CCN  14
DetectorReportPrinter.cpp:12   printDetectorReportGenericLine        132 NLOC  CCN  20
OccurrenceInspector.cpp:63     fillMagnitudeObservation              117 NLOC  CCN  19
```

Two of these are the 2026-10-25 note's B5 items `runRawTrigger` and
`buildSequenceAnalyzerReport`, unchanged since; the third B5 item,
`handleUsbLine` (669 lines on 2026-10-28), is gone (11j item 6 split it
into per-group handlers), and `applySeqStartToken` at CCN 58 is the
SEQ START ladder that split left in one piece. A brace-depth scan with
a single-line-signature regex misses both giants, because their
signatures wrap; the 10-28 note's table has the same blind spot (it lists
`buildSequenceAnalyzerReport` from the earlier note but not
`runRawTrigger`). Lizard does not.

The core's own tail is short and mostly known: `refreshReportDetail` 66
NLOC / CCN 40 (B5's "70 lines of nested ternaries"),
`updateTransientStage` 96 / 39 (the six-boolean acceptance rule; see
2.4), `FeatureHistory::getWindow` 149 / 38 and
`fillMagnitudeObservation` 117 / 19 (neither in the earlier notes),
`DetectionDiagnostics::onVerdict` 64 / 18,
`ResonantBehavior::handleOccurrenceVerdict` 70 / 21 and `update` 73 / 16
(B13), `I2sToneOutput::renderBlock` 86 / 30 (B5, "three interleaved
state machines").

### 2.3 State per class

```text
ScalarTransientDetector.h   68 members   (B4: the same event on two clocks)
ResonantBehavior.h          46 members
AnalyzerModeApp.h           30 members, methods in 11 .cpp files (A8)
```

Member count is the measure that best matches what the human read
called the real cost, and no standard tool reports it. The 10-28 note's
2.6 table had 45 / 37 / 25 at 2def60f; the Node's 37 is now spread over
the 11i role classes (`NodeSelfTest`, `NodeBaseline`, ...), the other two
are where they were (the Analyzer's grew by the 12.0.1 fixes).

### 2.4 Where the counts and the human read disagree

- **Comment density (7 %) says nothing about the comments.** The ratio
  counts lines; the value is in which lines. Twenty-one prose-contract
  headers and the why-comments on the tricky lines are what a reader
  uses, and no density figure sees them. (The first draft of this note
  had 3 %: an awk `\s` that mawk does not know, so only comments at
  column 0 counted. One more reason for a script, section 3.) A count of
  headers *with* a contract block (21 of the 61 that declare a class or
  struct) is the closer proxy; most of the 40 without one are small
  type or name-table headers that need none.
- **CCN overstates ladders and tables.** `systemResetReasonName` is CCN
  19 and trivially readable (one `switch`); `seqOutputWhenEnabled` CCN 14
  likewise. `&&`-chains count too, so the six-boolean acceptance rule in
  `updateTransientStage` puts a function a reader finds clear at CCN 39,
  level with the Analyzer's `update`. CCN is a fair ranking tool for the
  tail and a poor absolute threshold.
- **Line and file counts say nothing about the state problem.** The
  detector's `.cpp` is 422 lines of short functions and would pass any
  size gate; the 68 members are the difficulty.
- **Citation comments count as noise or as gold depending on the
  reader.** 217 `step N` / `issue #N` references in code bodies. For the
  owner they are the link into `docs/`; for anyone else they are
  opaque. No count settles that; see 4.4.

## 3. A tracked set

-> promoted to step 12.5 item 1, 2026-10-29

If any of this is kept, keep it small enough to print in one screen and
rerun on every pass, so a "did the pass help" question has a before and
after. Proposed: `tools/ci/readability.py` (lizard plus two greps), one
table, committed values nowhere (the script prints them; a pass doc
quotes them dated, the way bench numbers are quoted).

```text
A  functions with CCN > 20                       today 34   direction: down
B  functions with NLOC > 60                      today 46   down
C  functions with > 5 params                     today 13   down
D  members in the three largest classes          68/46/30   down
E  headers with an owns / does-not block,
   among the 61 that declare a class or struct   today 21   up
F  mean CCN, src/modes vs the rest               6.3 / 3.7  closer
G  lines > 120 (clang-format leaves literals)    today 12   flat
```

Not in the set, with the reason:

- Comment density: see 2.4; it would reward narration, which 11j item 4
  just deleted.
- Total lines: growth is the installation's doing (OTA, net, self-test,
  host replay since June), not a readability signal. The number worth
  watching is `src/modes/`'s share of it.
- Nesting depth: max 7 in one self-test function; the tail in A and B
  covers the same places.
- Test-line ratio (about 4,800 test lines to 24,100): host coverage is
  the 10-25 note's B16, a separate question.

Thresholds are not gates. A CI step that fails on A or B would have
blocked 11j item 6 (the split produced handlers over 60 lines on the way
to the finished shape) and would turn every reporting function into a
fight. Print the table in CI, fail on nothing, and read it when closing
a pass.

## 4. Measures to take, ranked by what the counts say

-> promoted to step 12.5 items 2-5, 2026-10-29

Nothing here is a commitment. Items that are picked up go into a step
of `roadmap-0-steps.md` per `docs/research/README.md`.

### 4.1 The two giants

`runRawTrigger` (620 NLOC, CCN 171, six manual malloc/free pairs per the
10-25 note) and `buildSequenceAnalyzerReport` (301 NLOC, CCN 103, ten
parameters). Between them they carry as much complexity as the next
eight functions of the tail together. Direction as in B5: `runRawTrigger` into setup /
capture / dump stages with one RAII buffer; the report builder into one
function per report section taking the `TrialDiagnostics` it already
receives. Both are Analyzer-only and host-untested, so the risk is a
changed SEQ transcript, which `tools/bench/seqcmp.py` on a replay corpus
catches (`pio run -e native-replay`).

### 4.2 The reporters

`printSystemHealth`, `printRbStatus`, `printDetectorReportGenericLine`,
`printAudioRunSummary`, `printSequenceInspectCanonical`: 100-200 NLOC
each, CCN 14-39, all linear print sequences. Long because each field is
a formatted token with its own ternary; not hard to read, tiring to
read, and the first place a transcript drift hides. A `LineWriter` that
takes `key=value` pairs (the 10-25 note's Part C lists the canonical
line format as something to keep) would halve them without changing a
byte of output. Lower value than 4.1 per line; higher per hour.

### 4.3 State: the detector and behavior members

The count with no tool behind it and the one the human read ranked
first. Options, from the 10-25 note's B2 / B4 / B13 directions:

- Group the detector's 68 members into the structs its own comments
  already name: `// Live gate state`, `// Candidate lifecycle state`,
  `// Occurrence emission state`, `// Detector report state`, `// Reject
  summary state` (`ScalarTransientDetector.h:89-168`). Five structs,
  reset by `= {}`, and a function's reads and writes become visible as
  `_candidate.peak`, `_report.generation`. No behavior change; the
  host replay corpus (`pio test -e native-test`) is the gate.
- The same for `ResonantBehavior`'s 46: timing deadlines, counters,
  pending request, rest state. `test_behavior_refractory` and
  `test_behavior_rest` gate it.

Measure D then reads as members per struct, which is what a reader
holds in their head at once.

### 4.4 Citations in code bodies

217 inline `step` / `issue` references, 11 of them bare (a citation and
nothing else on the line). Two options:

- Keep them. They are the project's working memory, and the docs rules
  (CLAUDE.md, "nothing is decided in an issue") make `docs/decisions/`
  the durable target; an inline `step 8b item 3` ages out when
  `roadmap-0-steps.md` is archived (it was, 2026-10-29, to `roadmap archive/`
  `roadmap-0-steps-1.md`).
- Rewrite each to say the *reason* in the comment and keep the citation
  as a trailing pointer: "close the candidate now, not at the sound's
  end, so one continuous tone gives one reject (step 8b item 3)" is the
  shape `updateTransientStage` already uses; the ones that are a bare
  "(step 8b)" are the noise.

The second is a comment-only pass over 11 lines, mechanical, and the
grep that finds them is `grep -nE '\((step [0-9]+[a-z]?( item
[0-9]+)?|issue #[0-9]+)\)\s*$'`. Not urgent; worth doing when the step
numbering next shifts.

### 4.5 The report twins

`AcceptedOccurrenceSummary` and `SelectedRejectSummary`, 22 identical
fields and a "compatibility alias" comment. One `OccurrenceSummary` and
a `rejectClass` / `detectorReason` pair on the reject side. Touches the
Analyzer printers and the host fixtures' expected lines; DET-018's
vocabulary cleanup is the natural place.

## 5. Suggested order, if any of this is taken up

1. The script (section 3), so 2-5 have a before.
2. 4.3 the detector's structs (highest reader value, host-gated, no
   output change).
3. 4.1 `buildSequenceAnalyzerReport`, then `runRawTrigger`
   (replay-gated).
4. 4.2 the reporters, with the `LineWriter`.
5. 4.5 with DET-018; 4.4 when the steps renumber.

## Open questions

- Is a count worth keeping that no CI step enforces? The argument for:
  the 10-25 and 10-28 notes each re-derived function sizes by hand and
  got different numbers for the same functions (brace-depth scans,
  wrapped signatures). A script makes the next note's table the same
  table.
- Member count per class has no tool; a 20-line python over headers is
  enough but is one more thing to keep honest. Alternative: clang-tidy
  (installed in the container) with
  `readability-function-size` and `readability-function-cognitive-
  complexity`, which also cover A and B and would read `.clang-tidy`
  from the repo. It does not count members either.
- Whether the Analyzer's reporters should be read as code at all, or as
  the transcript format's definition: if the latter, the canonical line
  format belongs in `notes_manual.md` and the printers are its
  implementation, and 4.2's `LineWriter` is the wrong fix; a table of
  `key, source field, format` the printers iterate is the right one.
