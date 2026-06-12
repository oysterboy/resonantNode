# Pass Y1 — Post-Review Docs + Tiny Code Cleanup

Status: cleanup pass after Code & Docs review  
Scope: docs + very small safe code cleanup  
Goal: align docs with current code after DetectionDiagnostics / legacy Analyzer cleanup, and remove obvious stale residue.

---

## 1. Update docs

Update:

```text
docs/implementation-status.md
docs/myspec.md
docs/roadmaps/roadmap_detection.md
Required doc corrections

Mark as completed / removed:

DetectionDiagnostics
AnalyzerSourceStageReport
AnalyzerFrequencyDiagnostic
AnalyzerScalarDiagnostic
legacy source-summary structs
patternResultQueueOverflowCount
BASE / CAP / VAL legacy tooling

Do not describe these as:

deferred
compatibility-only
still pending
current bridge

Correct current architecture wording:

DetectorReport is canonical detector-stage report.
AnalyzerReport is built through canonical Analyzer Bridge.
PatternMatcher is public pattern-stage boundary.
PatternAssembler / PatternRules are no longer public stages.
PatternResult is compact runtime-facing result.
2. Tiny code cleanup

Search:

makeInvalidResult
resetDiagnostics
resetDiagnosticsCounters
setDiagnosticsEnabled

Do only safe cleanup:

- delete unused makeInvalidResult if compile confirms unused
- do not rename resetDiagnostics* yet unless trivial and fully local
- keep setDiagnosticsEnabled if it still controls verbose detector/report stats
3. Do not change
detector behavior
thresholds
SEQ output semantics
PatternResult contract
PatternMatcher logic
Analyzer classification
Behavior / Output
Output report

Create/update:

docs/pass_Y1_post_review_cleanup.md

Include:

## Docs updated
## Removed stale references
## Code cleanup performed
## Left for later
## Build result
Acceptance
- docs no longer say DetectionDiagnostics / Analyzer legacy cleanup is deferred
- docs match current architecture
- obvious dead helper removed if safe
- no behavioral changes
- build passes


