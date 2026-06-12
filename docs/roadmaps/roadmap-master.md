# Roadmap Master

Status: future roadmap index
Scope: ResonantNode / Resonanzraum
Purpose: centralize future work that should no longer live inline in
`docs/myspec.md`.

---

## Detection / Analyzer Follow-Up

Primary detection backlog:

- detector / report consistency bug work
- behavior / output boundary clarification
- legacy removal and compatibility cleanup

See:

- [docs/roadmaps/roadmap_detection.md](/c:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/docs/roadmaps/roadmap_detection.md)
- [docs/archive/260512_detection-Refactor/reports/detection_refactor_final_cleanup.md](/c:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/docs/archive/260512_detection-Refactor/reports/detection_refactor_final_cleanup.md)

---

## Pattern / Detection Expansion

Future detector and pattern work:

- `PatternMatcher` multi-occurrence and competing-proposal matching
- `TargetBandStrength` full implementation
- `PulseSequence` / pulsed chirp grouping
- `CandidateCorrelator` / cross-source relation facts
- continuous tonal chirp trajectory
- glass chime / resonant decay
- woodblock / knock
- white-noise / broadband profile

---

## Params / Commands / Config

Future control-surface and tuning work:

- full `ParamRegistry`
- `CommandRouter`
- remote param update
- persistent config / params
- typed tuning structs for modules

---

## Behavior / Output

Future reaction and actuation work:

- `BehaviorRuntime` extraction
- `OutputProfile` / `OutputDispatcher`
- fleet / OTA / VEKTOR exposure

---

## Archive Boundary

Already-landed architecture notes live in:

- [docs/archive/260512_detection-Refactor/detection_roadmap.md](/c:/Users/malte/Documents/PlatformIO/Projects/ESP32_learn01/docs/archive/260512_detection-Refactor/detection_roadmap.md)

The active spec should stay focused on current architecture, not roadmap
inventory.
