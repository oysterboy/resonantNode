# Detection Refactor Roadmap

Status: future items only
Scope: ResonantNode / Resonanzraum Detection Refactor
Purpose: track detection-specific future work as a slice of
[docs/roadmaps/roadmap-master.md](/c:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/docs/roadmaps/roadmap-master.md).

This file only lists unresolved detection follow-up work. Landed architecture
notes live in the archive snapshot.

---

## Phase 3 - Remaining Implementation

### 3.1 Detector / report consistency

Status: open

Follow-up bug work, not a structural refactor:

- investigate why some clean summaries can still report detector acceptance
  inconsistently
- keep this separate from legacy-printer cleanup
- do not retune thresholds as part of this pass

### 3.2 Behavior / output boundary

Status: deferred

Future work may still be needed to make the behavior/output seam clearer:

- keep behavior focused on reaction policy
- keep output execution separate
- add a dispatcher only if it becomes clearly necessary

---

## Phase 4 - Legacy Removal

Status: deferred; see [docs/archive/260512_detection-Refactor/reports/detection_refactor_final_cleanup.md](/c:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/docs/archive/260512_detection-Refactor/reports/detection_refactor_final_cleanup.md) for the current remaining debt.

Remaining cleanup targets:

- retire `DetectionDiagnostics` when the compatibility bridge is no longer needed
- retire analyzer legacy compatibility structs only after their supported views
  are no longer required
- remove stale bridge comments and migration notes that no longer describe
  live code
- keep archive material historical instead of duplicating it in the active
  roadmap

Final success condition:

```text
No legacy output is treated as canonical.
No old source/report naming remains as architecture vocabulary.
No detector internals leak into PatternResult, AnalyzerReport, or Behavior.
Detection behavior remains explainable through compact runtime contracts and scoped reports.
```
