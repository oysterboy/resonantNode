# Code standards read, 2026-10-28

```text
Status:  promoted (2026-10-28)
Became:  step 11j (owner 2026-10-28: line limit 120, go now; issue #73);
         done 2026-10-28: items 1-6 of section 3 landed (c907048..51d88d6, the
         "->" lines there), clang-format check in CI; 11j closed, issue #73 closed
Lab:     none; a code reading at main 2def60f (after the 11i N1-N5 Node
         splits), from a cloud container: grep / awk counts over src/ and
         lib/, no build and no hardware. Every file:line below was read
         before it was written down; line numbers are as of 2def60f.
```

A read of the tree against a short, generic standards list the owner
supplied: readability first, KISS, DRY, YAGNI, descriptive names,
consistent formatting (indentation, line length, file organization), and
comments that say why rather than what. It is not a second architecture
review. `architecture-readability-review-2026-10-25.md` (the earlier note)
already covers the mode layer's big functions, parsing and cross-mode
duplication in depth; where a finding here is one of its items the item is
named and only what is new at 2def60f is added. Nothing here is a
commitment.

Calibration: the core (detection, behavior, `lib/ParamRegistry`) meets the
list. The mode layer (`src/modes/`) is where readability, DRY and
formatting slip, and the slips are mechanical: a formatter and three small
helper merges would clear most of them.

## 1. Where the tree meets the list

### Names

One convention, held everywhere: PascalCase types and files, camelCase
verb methods (`handleOccurrenceVerdict`, `scheduleNextIdle`, `resetState`),
`_member` prefix (430 distinct `_x` members in headers, zero `m_x`),
`k`-prefixed constants (160), no ALL_CAPS non-macro constants, no `using
namespace`. The only snake_case functions are in the namespaced helper
sets (`text_parse::`, `serial_line::`), which is a deliberate style for
free functions, not drift.

### Comments say why, and cite

`src/app/RuntimeDefaults.h:81-89` explains why `kDcBlockerOutputScale`
is the literal 0.588, which decision set it, and which host test fails if
the chirp or sample rate changes under it. `src/net/NodeNet.cpp:58`,
`src/behavior/ResonantBehavior.cpp:149` and
`src/modes/analyzer/AnalyzerSequenceSession.cpp:96` each cite a decision,
a date or an issue. `ResonantBehavior.cpp:5-35` opens with an owns /
does-not-own block. Zero `TODO`, `FIXME`, `HACK` or `XXX` in `src/` and
`lib/`.

### Numbers are named and in one place

`RuntimeDefaults.h` holds the grid, rates and defaults with a reason per
block; `DetectionProfile.h` holds the thresholds with a reason per line.
The float literals left in `.cpp` files are named constants
(`I2sToneOutput.cpp:16-18`, `AudioSourceI2S.cpp:301-302`,
`AudioSignal.cpp:27`) or appear in comments. One stray:
`ResonantNodeApp.cpp:580` passes `0.005f` inline (the earlier note's B7
already lists `configureI2SParameters`'s 0.5 / 0.005 / 20 as duplicated
between Node and Analyzer).

### Boundaries are enforced, not only documented

CI (`.github/workflows/build.yml`) builds every env, runs the three native
test envs (18 suites under `test/`), checks include direction
(`tools/ci/include_direction.py --strict`), and checks that the test lists
agree. The earlier note's Part C stands.

## 2. Where it does not

### 2.1 Function size (readability, KISS); earlier note B5, still open

```text
AnalyzerCommands.cpp:110    AnalyzerApp::handleUsbLine           669 lines
AnalyzerModeApp.cpp:826     buildSequenceAnalyzerReport          ~312 lines
AnalyzerModeApp.cpp:629                                          137 lines
EmitterApp.cpp:320                                               113 lines
ResonantNodeApp.cpp:591                                          110 lines
ResonantBehavior.cpp:200    update                                88 lines
ResonantBehavior.cpp:114    handleOccurrenceVerdict               84 lines
```

(Brace-depth count, so approximate.) `handleUsbLine` is one `strcmp` /
`equalsIgnoreCase` ladder (30 compares in the file). No host test reaches
command dispatch in any mode: `test_text_parse` covers the helpers,
`test_param_commands` the PARAM path only.

### 2.2 Formatting: nothing enforces it (new)

There is no `.clang-format` and no `.editorconfig`. The indentation is
4 spaces by convention (9,275 lines at 4, 4,189 at 8, 1,892 at 12), with
44 lines at 2 and 40 at 6, which is the drift a formatter would remove.
Inside `handleUsbLine` sibling `SEQ` branches sit at different depths:
`AnalyzerCommands.cpp:310` (`FREQUPDATEEVERYSAMPLES`) at 4 spaces,
`:328` (`MODE`) at 8, and `:332` is one level off from its own block.
The earlier note's B5 saw the same drift at c0b75e9; it survived 11e / 11f
because nothing checks it.

Line length, `src/` and `lib/`, 24,197 lines:

```text
over  80 chars   2,223
over 100 chars     957
over 120 chars     278
longest            353   DetectionProfile.h:117
```

The long lines are mostly trailing comments that carry a decision's
history on the assignment (`DetectionProfile.h:117-118, 144-145`). The
history belongs in `docs/decisions/` with a short pointer on the line.
70 files carry trailing whitespace.

### 2.3 DRY

- `equalsIgnoreCase` / `startsWithTokenIgnoreCase` defined three times:
  `src/app/TextParse.h:99`, `src/detection/analyzer/AnalyzerText.h:14,18`,
  `src/modes/resonant/ResonantNodeInternal.h:17,21`. Earlier note B7
  listed two; the third (`TextParse.h`) was added by 11e as the intended
  home, and the other two were not removed.
- The serial line-reading loop (`feed` / `tooLong` / `line_too_long` /
  dispatch) is written four times: `EmitterApp.cpp:116-128` and
  `:131-142`, `ResonantNodeCommands.cpp:17-30`,
  `AnalyzerCommands.cpp:95-107`. 11e gave them one `LineReader` (B6 done)
  but not one poll routine.
- `ResonantBehavior.cpp`: `_lastDecision` and `_lastBlockReason` are set
  as a pair 15 times (earlier note B13 counted twelve). One
  `setDecision(BehaviorDecision)` removes all of them.
- `ScalarTransientDetector::resetState` (`ScalarTransientDetector.cpp:17`)
  inlines the bodies of `resetAcceptedOccurrenceSummary` and
  `resetSelectedRejectSummary` instead of calling them, and clears
  `_reportDetail` three times (B2, partly done; the detector half is still
  as described there).
- `AnalyzerCommands.cpp:740-749` and `:750-759` (`EMIT CHIRP` / `EMIT
  PULSES`) are identical except the echoed name (B7, listed, open).
- `DetectionProfile.h` writes the same twelve module fields per
  inspection module, three modules per profile, two profiles. A
  `magnitudeModule(target, stream, anchor, mode, pre, post, strong, medium,
  weak)` helper would make each module one line and the diffs between
  profiles readable.

### 2.4 Comments that narrate (new)

`DetectionProfile.h:134-148` comments every field with what the
assignment already says: `.enabled = true; // Enable the module in the
matcher.`, `.windowPreMs = 0; // No look-back before the anchor.` The
second profile (`:228-250`) has no comments at all, so the file is
inconsistent as well as noisy. Commented-out tuning lines remain at
`DetectionProfile.h:237` and `:251`. (The earlier note's B9 covered
comment drift; the narration is a different problem: not wrong, not
useful.)

### 2.5 YAGNI and leftovers (new)

- `critique.md` at the repo root opens with "a confirmed correctness bug
  in the stable detector path" in `FrequencyMatchDetector`, removed
  2026-10-22 (`decisions/2026-10-22-remove-frequencymatch.md`), says the
  tree is about 19,000 lines (it is 24,000), and nothing in `docs/` or
  `CLAUDE.md` links to it. It reads as current to a newcomer. Archive it
  under `docs/refactors/archive/` with an `Archived:` line, or delete it.
- `AnalyzerModeApp.cpp:1003`: `report.occurrences.contrast = 0.0f; //
  was the Freq occurrence's band contrast`. A report field kept
  hard-wired to zero after the detector that produced it was removed
  (DET-018 territory: vestigial vocabulary).
- `DetectionProfile.h:144`: `strongPeakThreshold = 300000.0f` as a
  "never fires" sentinel, with a comment explaining that Strong is unused
  for contrast. A disabled class is clearer than a value above the domain.
- There is no root `README.md`; the root holds `CHANGELOG.md`,
  `CLAUDE.md` and `critique.md`, and onboarding runs through `CLAUDE.md`
  plus 198 markdown files under `docs/`. A ten-line `README.md` pointing
  at `CLAUDE.md`, `docs/notes_manual.md` and `docs/specs/myspec.md` is
  what a newcomer expects to find.

### 2.6 Class size

```text
ResonantBehavior.h    45 members   64 methods
ResonantNodeApp.h     37 members   55 methods
AnalyzerModeApp.h     25 members   78 methods
```

(Header line counts, so approximate.) The 11i N1-N5 commits are splitting
`ResonantNodeApp` by role; `AnalyzerModeApp` has not had the same pass
(earlier note A7 / A8).

## 3. Suggested order, if any of this is taken up

1. `.clang-format` (4 spaces, 100 or 120 columns, current brace style) and
   `.editorconfig`; one formatting-only commit; a CI step that fails on
   drift. This closes 2.2 and keeps B5's indentation finding closed.
   -> done 2026-10-28 (11j 1): c907048 (config, CI job, clang-format 18.1.8), 943913e (the reformat, 98 files).
2. Delete the two helper copies (2.3, first item) and give the four poll
   loops one routine next to `LineReader`.
   -> done 2026-10-28 (11j 2): 2404dc2 + 9ff8fb5 (AnalyzerText.h deleted; serial_line::pollLines).
3. `setDecision()` in `ResonantBehavior`; `resetState` calls its own
   helpers in `ScalarTransientDetector`.
   -> done 2026-10-28 (11j 3): 070f643.
4. Move the inline decision history out of `DetectionProfile.h` to
   `docs/decisions/` pointers, delete the narration comments and the two
   commented-out lines, add the module helper.
   -> done 2026-10-28 (11j 4): 7984937 (magnitudeModule / binShapeModule; values identical).
5. Archive `critique.md`; add `README.md`; drop the dead `contrast`
   field with DET-018.
   -> done 2026-10-28 (11j 5): b2a8e88; the contrast Strong 300000 stays (no disabled class in the inspector).
6. `handleUsbLine` into per-group handlers (B5 / B6 direction) with a
   host test for dispatch. Biggest item, last, because 1-5 make it
   smaller and safer.
   -> done 2026-10-28 (11j 6): 6514fe3; the host test covers routing and token checks (handlers need the board).

## Open questions

- Line limit: 100 or 120? The spec and guide wrap near 100; the code's
  long lines are 100-130 once the comment histories move out. 120 avoids
  a second reflow of the whole tree.
- Whether the one-shot format commit should wait for the 11i Node split
  to land, so the split's diffs stay readable.
