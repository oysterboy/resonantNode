# Detection Refactor Roadmap — Legacy Printer Removal Pass

Status: implementation roadmap / Codex pass sequence  
Scope: ResonantNode / Resonanzraum Detection Refactor  
Position: after detector/report canonicalization and SEQ clean-path split  
Purpose: identify legacy printer functions still in use, decide rebuild-on-clean-path vs delete, then execute safe removal

---

## Goal

This pass is about the Analyzer / SEQ printing surface, not detector behavior.

We want to:

```text
1. identify every legacy printer/helper still in active use
2. decide whether each one should:
   - stay legacy temporarily
   - be rebuilt on the clean path
   - be deleted outright
3. run a removal pass only after the decision map is explicit
```

This is not a threshold-tuning pass and not a detector-logic rewrite.

---

## Core rule

```text
If an output cannot be produced from canonical detector/pattern/analyzer facts,
it is not part of the clean output path.
```

Canonical clean path means:

```text
DetectorReport
PatternResult
AnalyzerReport canonical classification
expected trial/window facts
```

Legacy-only facts must not be silently copied into new clean printers just to preserve old text.

---

## Main questions this pass must answer

### 1. Which legacy printer functions are still active?

Meaning:

```text
- reachable from commands
- called from trial finalization
- called from summary/stop/status flows
- called from base/capture/value output paths
```

### 2. Which ones are only compatibility?

Meaning:

```text
- explicit *_LEG outputs
- temporary developer diagnostics
- old mixed source-summary printers
- old audio/freqband/perf dumps
```

### 3. Which ones should be rebuilt on the clean path?

Only if:

```text
- the output is still wanted long-term
- the output can be sourced from canonical facts
- the output belongs to clean SEQ_INSPECT / SEQ_EXPLAIN / SEQ_SUMMARY / future SEQ_TRIAL
```

### 4. Which ones should be removed?

Remove if:

```text
- no active command/mode uses them
- only dead compatibility remains
- they duplicate a clean printer
- they require legacy facts we do not want to preserve
```

---

## Global rules

```text
- Runtime behavior change: expected none.
- No threshold/profile/timing tuning.
- No detector behavior changes.
- No Analyzer classification changes unless explicitly required by clean ownership.
- No accidental output deletion before command/callsite inventory is complete.
- Clean path may grow; legacy path should shrink.
- Compile after each implementation step.
- Commit after each pass.
```

Standard checkpoint:

```bash
platformio run -e esp32dev-analyzer
git status
git diff --stat
```

---

# Pass T1 — Legacy Printer Inventory

## Goal

Create a hard inventory of all legacy printer functions/helpers and mark which are still in use.

---

## Main code areas

```text
src/modes/analyzer/AnalyzerApp.h
src/modes/analyzer/AnalyzerCommands.cpp
src/modes/analyzer/AnalyzerSequenceSession.cpp
src/modes/analyzer/AnalyzerSequenceHelpers.cpp
src/modes/analyzer/AnalyzerLegacyReporting.h
src/modes/analyzer/AnalyzerLegacyReporting.cpp
```

Search terms:

```text
legacyPrint
SEQ_*_LEG
printSequenceSummaryClean
printSequenceInspectCanonical
printSequenceExplainCanonical
printSystemHealth
SIGNALCHECK
AUDIO run
FREQBAND
OCCURRENCE summary
```

---

## Required output

Create:

```text
docs/legacy_printer_inventory.md
```

Required table columns:

```text
Function
File
Primary output label(s)
Called from
Command/mode reachable?
Uses canonical facts only?
Uses legacy facts?
Classification
Recommended action
```

Allowed classifications:

```text
CLEAN_ACTIVE
LEGACY_ACTIVE
LEGACY_COMPAT_ONLY
REBUILD_ON_CLEAN_PATH
DELETE_NOW
DELETE_AFTER_REBUILD
UNKNOWN
```

---

## What to inspect

For each printer/helper, answer:

```text
- Is it directly user-visible?
- Is it called only from another legacy helper?
- Does it print canonical detector/pattern/analyzer truth?
- Does it print DetectionDiagnostics / legacy source-summary / legacy counters?
- Is it part of:
  - clean inspect
  - clean explain
  - clean summary
  - legacy inspect/explain/summary
  - system/perf/debug dump
```

Do not stop at `legacyPrint*` names only. Include helper printers that feed them.

---

## Acceptance criteria

T1 is accepted if:

```text
- every active analyzer printer path is inventoried
- direct callsites are identified
- command/mode reachability is explicit
- canonical-vs-legacy data ownership is noted
- each function has a recommended action
```

---

# Pass T2 — Decision Map: Rebuild Clean Path vs Remove

## Goal

Turn the inventory into an explicit decision map before deleting anything substantial.

---

## Decision rule

### Rebuild on clean path

Choose this only if all are true:

```text
- output is still wanted
- output belongs to long-term analyzer UX
- output can be sourced from canonical facts
- output is not just historical mixed diagnostics
```

Examples likely in this bucket:

```text
clean SEQ_SUMMARY
clean SEQ_INSPECT
clean SEQ_EXPLAIN
future generic SEQ_TRIAL if rebuilt canonically
```

### Keep legacy temporarily

Choose this only if:

```text
- still useful for developer comparison
- still depends on legacy-only facts
- we are not ready to rebuild it canonically
```

It must stay visibly legacy.

### Remove

Choose this if:

```text
- function is unused
- function duplicates a clean output
- function is only a transitional helper with no future value
- keeping it would preserve bad ownership or bad vocabulary
```

---

## Required output

Create:

```text
docs/legacy_printer_decision_map.md
```

Required sections:

```text
# Legacy Printer Decision Map

## Purpose
## Clean Printers To Keep
## Legacy Printers To Keep Temporarily
## Printers To Rebuild On Clean Path
## Printers To Remove Now
## Printers To Remove After Rebuild
## Command / Mode Implications
## Data Ownership Rules
## Blockers
## Recommended Removal Pass
```

For each kept/rebuilt/removed printer, include the concrete function names.

---

## Specific rule for mixed system/perf printers

For output like:

```text
SEQ tuning
SEQ freqmatch
FREQBAND runtime
AUDIO summary
OCCURRENCE summary
AUDIO run
FREQBAND config/profile/freshness
```

decide explicitly whether they are:

```text
- long-term clean system diagnostics
- legacy developer diagnostics
- not part of SEQ clean truth and therefore removable from clean-path commands
```

Do not leave them half-canonical by accident.

---

## Acceptance criteria

T2 is accepted if:

```text
- every inventoried legacy printer has an explicit keep/rebuild/remove decision
- clean outputs are separated from legacy/system/perf outputs
- command implications are documented
- the next deletion pass can proceed without guesswork
```

---

# Pass T3 — Printer Removal Pass

## Goal

Remove the legacy printer functions/helpers that T2 marked for deletion, and only those.

---

## Execution order

Do not treat T3 as one large delete-everything pass.

Run it in this order:

```text
T3a — Split sequence final output
T3b — Rebuild canonical SEQ_TRIAL
T3c — Retire source-summary legacy printers
T3d — Retire legacy comparison printers
T3e — Rebuild neutral system/config/status/trial-support printers
```

---

## Pass T3a — Split sequence final output

### Goal

Separate final sequence output into:

```text
- clean summary truth
- explicit system/perf diagnostic bundle
```

### Scope

Completed in current code. No remaining concrete targets in this sub-pass.

### Desired end state

```text
SEQ STOP
  -> clean summary output remains explicit
  -> optional diagnostics bundle is clearly system/debug, not clean truth
```

### Acceptance criteria

```text
- final sequence output is no longer conceptually mixed
- clean SEQ_SUMMARY ownership stays explicit
- system/perf bundle is clearly separated
```

---

## Pass T3b — Rebuild canonical SEQ_TRIAL

### Goal

Replace the current mixed legacy trial printer with a clean canonical trial
printer.

Completed in current code. No remaining concrete targets in this sub-pass.

### Rule

The rebuilt trial printer may use only:

```text
DetectorReport
PatternResult
AnalyzerReport canonical classification
expected trial/window facts
```

### Acceptance criteria

```text
- a clean SEQ_TRIAL path exists
- old mixed trial summary data is not required by the clean trial path
- legacy trial path can remain only as explicit comparison if still needed
```

---

## Pass T3c — Retire source-summary legacy printers

### Goal

Delete the legacy source-summary reporting family after clean trial / inspect /
explain paths are sufficient.

### Primary deletion targets



### Acceptance criteria

```text
- source-summary compatibility output is removed or explicitly isolated
- no clean printer depends on legacy source-summary carriers
```

---

## Pass T3d — Retire legacy comparison printers

### Goal

Remove explicit legacy comparison printers when they are no longer needed.

### Primary targets



### Rule

Do not remove these until:

```text
- clean replacements are accepted, or
- we explicitly decide the comparison surface is no longer supported
```

### Acceptance criteria

```text
- any remaining legacy comparison surface is intentional and explicit
- removed comparison outputs are truly unreachable
```

---

## Pass T3e — Rebuild neutral system/config/status/trial-support printers

### Goal

Rebuild the printer functions that are still useful, but should not stay under
legacy SEQ ownership or mixed clean-truth semantics.

### Intent

These outputs should become one of:

```text
- neutral system/status/config diagnostics
- clean canonical trial output
- explicit non-canonical developer tooling
```

They should not remain:

```text
- half-legacy defaults
- implicit clean truth
- mixed source-summary output
```

### Acceptance criteria

```text
- each rebuilt printer has explicit ownership
- clean truth printers remain canonical-only
- system/perf/config printers are clearly named and separated
```

---

## Allowed changes

```text
- delete unused legacy printer functions
- delete dead helper functions used only by removed printers
- remove dead command aliases if T2 says so
- update comments/docs/help text
- keep temporary wrappers only when T2 explicitly says keep
```

## Not allowed

```text
- broad rewrite without decision map
- detector behavior changes
- threshold/profile changes
- sneaking legacy-only facts into clean printers
- deleting still-supported legacy commands without documenting it
```

---

## Required documentation

Create:

```text
docs/legacy_printer_removal.md
```

Required sections:

```text
# Legacy Printer Removal

## Purpose
## Removed Functions
## Removed Helpers
## Commands / Modes Changed
## Kept Legacy Printers
## Rebuilt Clean Printers
## What Still Depends On Legacy
## Compile Result
## Runtime Sanity Result
## Remaining Printer Debt
```

Also update:

```text
docs/implementation-status.md
docs/detection_refactor_final_cleanup.md
docs/roadmaps/roadmap_detection.md
```

---

## Runtime check

If hardware is available, run at least:

```text
SEQ INSPECT
SEQ EXPLAIN
SEQ SUMMARY
SEQ SUMMARY LEG   (if still supported)
SEQ STOP
```

Verify:

```text
- clean outputs still work
- legacy outputs only appear where explicitly requested
- removed printers are truly unreachable
```

---

## Acceptance criteria

T3 is accepted if:

```text
- delete-now legacy printers are gone
- clean outputs still compile and run
- legacy outputs only remain where intentionally supported
- docs reflect the new printer surface
```

---

## Suggested commit sequence

```text
T1: Docs/AnalyzerCleanup [T1] inventory legacy printer functions
T2: Docs/AnalyzerCleanup [T2] map clean rebuild vs printer removal
T3: AnalyzerCleanup [T3] remove obsolete legacy printer paths
```

---

## Recommended next step

Start with T1 only.

Do not jump straight to T3.

The whole point of this pass is to avoid deleting printer code blindly before we know:

```text
- what is still active
- what is still wanted
- what belongs on the clean path
- what should disappear completely
```
