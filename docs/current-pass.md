# Codex Task: Detection Roadmap v0.3 — Pass 8: Add Detection Mode Switch

Use `docs/detection-roadmap-v0-2-implementation-brief.md` as the architecture source of truth, but apply the updated Roadmap v0.3 naming/rule corrections from the latest refactor notes.

## Scope

Resonant Node detection integration scaffold.

Add a detection mode switch, but do not fully replace the current Node detection path yet.

This pass prepares Node so the next pass can integrate `DetectionRuntime` safely.

## Goal

Add an explicit RB detection mode enum and serial command so we can switch between:

```txt
AmpLegacy
RoadmapFrequencyFirst
RoadmapFrequencyOnly
```

The purpose is to compare old and new detection paths without reflashing.

---

## Detection Modes

Add:

```cpp
enum class DetectionMode {
    AmpLegacy,
    RoadmapFrequencyFirst,
    RoadmapFrequencyOnly
};
```

Preferred location:

```txt
src/modes/resonant/node.h
```

Use a private member:

```cpp
DetectionMode _detectionMode = DetectionMode::AmpLegacy;
```

For this pass, keep the default as `AmpLegacy`.

The next integration pass may switch default later.

---

## Meaning

```txt
AmpLegacy:
  old current AMP-first path
  current behavior remains unchanged

RoadmapFrequencyFirst:
  future DetectionRuntime path
  frequency candidates preferred
  AMP candidates allowed as fallback/support/comparison

RoadmapFrequencyOnly:
  future DetectionRuntime path
  AMP ignored for behavior
  AMP may still be used for diagnostics later
```

In this pass, `RoadmapFrequencyFirst` and `RoadmapFrequencyOnly` may be accepted as modes but should not yet change detection behavior unless DetectionRuntime integration is already trivial and safe.

Prefer no runtime behavior change in this pass.

---

## Serial Command

Add a command to `Node::handleSerialLine(...)`:

```txt
RB DETECT mode=legacy
RB DETECT mode=freq
RB DETECT mode=freqonly
```

Also allow a status query:

```txt
RB DETECT
```

Suggested outputs:

```txt
RB DETECT mode=legacy
RB DETECT mode=freq
RB DETECT mode=freqonly
```

Add to `RB help` output:

```txt
RB CMD: RB DETECT mode=legacy|freq|freqonly
```

---

## Helper Functions

Add small helper methods if useful:

```cpp
const char* detectionModeName() const;
bool setDetectionModeFromName(const char* name);
```

or static free helpers in `node.cpp`.

Accepted mode strings:

```txt
legacy
amp
ampLegacy
freq
frequency
roadmap
freqfirst
frequencyfirst
freqonly
frequencyonly
```

Map:

```txt
legacy / amp / ampLegacy
→ AmpLegacy

freq / frequency / roadmap / freqfirst / frequencyfirst
→ RoadmapFrequencyFirst

freqonly / frequencyonly
→ RoadmapFrequencyOnly
```

Keep parsing simple and consistent with existing command parsing style.

---

## Behavior in This Pass

This pass should not yet route frames through `DetectionRuntime`.

It should only add:

```txt
- enum
- member state
- serial command
- help output
- summary output if useful
```

Runtime detection should remain the same in all modes for now unless an earlier implementation already supports `DetectionRuntime` safely.

If modes do not change runtime yet, add a comment:

```txt
DetectionRuntime integration happens in the next pass.
For now, detection mode is stored and reported, but AmpLegacy remains the active runtime path.
```

---

## Optional Summary Output

If `RB summary` prints current config, add:

```txt
detectMode=<name>
```

Example:

```txt
RB SUMMARY ... detectMode=legacy
```

Do this only if it is simple and consistent with existing summary formatting.

---

## Anti-Wrapper Rule

This pass does not clean old direct detection usage yet.

But it creates the control seam for the cleanup.

Next pass should make:

```txt
RoadmapFrequencyFirst
RoadmapFrequencyOnly
```

route through:

```txt
DetectionRuntime
```

and should ensure that `Node` no longer directly uses candidate builders for roadmap behavior-path detection.

---

## Constraints

Do not:

- change current default runtime behavior
- change ResonantBehavior
- change Analyzer runtime behavior
- change output behavior
- tune thresholds
- remove legacy code
- remove candidate builders
- refactor PatternRules
- refactor PatternAssembler
- add DetectionStrategy/Profile
- add FieldState
- implement complex pattern grouping
- make roadmap mode default yet

---

## Acceptance Criteria

- `DetectionMode` enum exists.
- Node stores current detection mode.
- Default mode remains `AmpLegacy`.
- `RB DETECT mode=legacy` works.
- `RB DETECT mode=freq` works.
- `RB DETECT mode=freqonly` works.
- `RB DETECT` reports current mode.
- `RB help` includes the command.
- Runtime behavior remains unchanged.
- Project compiles.

---

## Notes for Next Pass

Next pass should integrate `DetectionRuntime` into Node.

Target future roadmap-mode flow:

```cpp
_detection.observeFrame(frame, frequencyEvidence, nowMs);

DetectionPipeline::PatternResult result;
while (_detection.popPatternResult(result)) {
    _behavior.handlePatternResult(result, nowMs);
}
```

In roadmap modes, Node should stop manually building PatternResults from old candidate builders.
