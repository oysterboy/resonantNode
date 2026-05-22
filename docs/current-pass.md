# Codex Instruction — Rename Signal Layer to Occurrence + FreqAmp Profile to TonalPulse

Scope: naming-only refactor.

Goal:

1. Rename the source-level bounded detection objects from `Signal*` terminology to `Occurrence*`.
2. Rename the `FreqAmp` profile to `TonalPulse` throughout the codebase and docs.
3. Remove or avoid decorative/duplicated global profile fields if they only mirror hardwired source behavior.
4. Do not change runtime behavior, detector thresholds, pattern rules, analyzer semantics, or behavior decisions.

This pass is a mechanical naming / terminology alignment pass.

---

## Global Rules

Do:

```text
- rename folders, files, headers, classes, structs, enums, functions, variables, labels, logs, comments, and docs
- dont tuch files in archive
- update includes and build references
- update namespace/type references consistently
- update analyzer output labels only where they refer to renamed architecture objects/profile names
- run grep checks before and after
- build/compile after the rename
```

Do not:

```text
- change detector logic
- change PatternRules logic
- change thresholds
- change behavior decisions
- change Analyzer classification semantics
- change output timing
- change profile behavior
- add new architecture
- partially alias old names unless needed temporarily for compile migration
```

No compatibility aliases should remain after the pass unless absolutely necessary.

If any alias is kept, add a TODO explaining why and when it can be removed.

---

# Part A — Rename Signal Layer to Occurrence

## Intent

The current `Signal*` terminology is ambiguous because `AudioSignal` / `FeatureStream` represent continuous signal material, while `SignalCandidate` / `InspectedSignal` are bounded source-level detected objects.

Use `Occurrence` for the bounded source-level detected thing.

Architectural distinction:

```text
Signal:
    continuous audio-derived material / stream state

Occurrence:
    bounded source-level acoustic happening derived from one evidence path

PatternCandidate:
    assembled pattern-level candidate ready for PatternRules

PatternResult:
    semantic result consumed by Behavior / Analyzer
```

Keep `AudioSignal` / `audioSignal` terminology untouched where it means the continuous audio signal layer.

This pass is about the discrete detection layer.

---

## Required Rename Map

Apply consistently across folders, files, types, functions, variables, comments, labels, docs, and logs.

```text
SignalCandidate        → Occurrence
SignalSource           → OccurrenceSource
SignalEmitter          → OccurrenceSource
AmpSignalEmitter       → AmpOccurrenceSource
FrequencySignalEmitter → FrequencyOccurrenceSource
SignalInspector        → OccurrenceInspector
InspectedSignal        → InspectedOccurrence
```

Plural and collection names:

```text
SignalCandidates          → Occurrences
InspectedSignals          → InspectedOccurrences
signalCandidates          → occurrences
inspectedSignals          → inspectedOccurrences
signalCount               → occurrenceCount
recentSignalCount         → recentOccurrenceCount
recentAcceptedSignalCount → recentAcceptedOccurrenceCount
lastSignalMs              → lastOccurrenceMs
lastInspectedSignalMs     → lastInspectedOccurrenceMs
```

Source-specific labels, if present:

```text
AmpSignal       → AmpOccurrence
FrequencySignal → FrequencyOccurrence
```

Only apply source-specific replacements if those identifiers exist.

---

## Folder / File Rename Guidance

If the codebase has a folder named:

```text
src/detection/signals/
```

rename it to:

```text
src/detection/occurrences/
```

Example file renames if present:

```text
SignalCandidate.h        → Occurrence.h
SignalSource.h           → OccurrenceSource.h
SignalEmitter.h          → OccurrenceSource.h
AmpSignalEmitter.h       → AmpOccurrenceSource.h
FrequencySignalEmitter.h → FrequencyOccurrenceSource.h
SignalInspector.h        → OccurrenceInspector.h
InspectedSignal.h        → InspectedOccurrence.h
```

Update all `#include` paths.

If PlatformIO/build config references file paths explicitly, update those too.

---

## API / Function Rename Guidance

Rename functions and methods where the meaning is now occurrence-based.

Examples:

```text
emitSignalCandidate(...)      → emitOccurrence(...)
tryEmitSignal(...)            → tryEmitOccurrence(...)
inspectSignal(...)            → inspectOccurrence(...)
onSignalCandidate(...)        → onOccurrence(...)
recordSignalCandidate(...)    → recordOccurrence(...)
recentSignalCount()           → recentOccurrenceCount()
lastSignalMs()                → lastOccurrenceMs()
```

Do not rename unrelated uses of `signal` where it refers to continuous audio signal material.

Keep names like these if they refer to continuous signal:

```text
AudioSignal
audioSignal
signalLevel
signalFrame
signalMagnitude
signalProcessing
signal stream
```

---

## Logs / Analyzer / Docs Labels

Update log labels where they refer to bounded detection objects.

Examples:

```text
signals=           → occurrences=
signal_count=      → occurrence_count=
signal_source=     → occurrence_source=
inspected_signal=  → inspected_occurrence=
```

Do not rename labels where `signal` means continuous audio signal level or signal processing.

Analyzer report sections should update similarly if they exist:

```text
SignalObservation      → OccurrenceObservation
signals                → occurrences
primarySignal          → primaryOccurrence
acceptedSignals        → acceptedOccurrences
rejectedSignals        → rejectedOccurrences
```

Only rename Analyzer types if they are part of the bounded-object layer.

---

## FieldState Rename Guidance

If `FieldState` contains signal-count fields, update them:

```text
recentSignalCount          → recentOccurrenceCount
recentAcceptedSignalCount  → recentAcceptedOccurrenceCount
lastSignalMs               → lastOccurrenceMs
lastInspectedSignalMs      → lastInspectedOccurrenceMs
```

Meaning remains identical.

Do not change `FieldState` behavior, thresholds, or semantics.

---

## Spec / Docs Flow Update

Update architecture flow text from:

```text
FeatureStreams
→ SignalEmitters / CandidateSources
→ SignalCandidates
→ SignalInspector
→ InspectedSignals
→ PatternAssembler
→ PatternCandidates
→ PatternRules
→ PatternResults
```

to:

```text
FeatureStreams
→ OccurrenceSources
→ Occurrences
→ OccurrenceInspector
→ InspectedOccurrences
→ PatternAssembler
→ PatternCandidates
→ PatternRules
→ PatternResults
```

If the docs mention `SignalEmitter / CandidateSource`, prefer the new term:

```text
OccurrenceSource
```

---

## Part A Post-Rename Grep Checks

After renaming, run:

```bash
grep -R "SignalCandidate\|InspectedSignal\|SignalInspector\|SignalEmitter\|AmpSignalEmitter\|FrequencySignalEmitter\|SignalSource" -n src include docs test 2>/dev/null
```

Remaining matches are allowed only in archived historical docs.

Active code and active docs should not use these names.

Also check lowercase labels:

```bash
grep -R "signalCandidate\|inspectedSignal\|signalEmitter\|signal_source\|signal_count\|recentSignalCount\|lastSignalMs" -n src include docs test 2>/dev/null
```

Update or justify all active matches.

---

# Part B — Rename FreqAmp Profile to TonalPulse

## Intent

Rename the current `FreqAmp` profile to `TonalPulse`.

This is a profile/name change only.

Do not change profile behavior, thresholds, rules, detector logic, analyzer semantics, or behavior decisions.

---

## Mandatory Preflight Search

Before applying the rename, search for existing usage of `TonalPulse`, the earlier accidental `RonalPulse` spelling, and close variants.

Run:

```bash
grep -R "TonalPulse\|tonalPulse\|tonalpulse\|TONAL_PULSE\|RonalPulse\|ronalPulse\|ronalpulse\|RONAL_PULSE" -n src include docs test 2>/dev/null
```

If existing `TonalPulse` usage is found:

```text
- report the matches
- determine whether they are active code, docs, archive, or stale references
- merge carefully if they already describe the same profile
- do not create a second competing TonalPulse concept
```

If `RonalPulse` appears:

```text
- treat it as stale/wrong spelling unless explicitly documented as historical archive
- rename active occurrences to TonalPulse
```

The requested target name is:

```text
TonalPulse
```

Do not use:

```text
RonalPulse
```

---

## Required Rename Map

Apply consistently:

```text
FreqAmp          → TonalPulse
freqAmp          → tonalPulse
freqamp          → tonalpulse
FREQ_AMP         → TONAL_PULSE
FREQAMP          → TONALPULSE
FreqAmpProfile   → TonalPulseProfile
```

If code uses enum values:

```text
DetectionProfileKind::FreqAmp       → DetectionProfileKind::TonalPulse
ProfileFeatureSetKind::FreqAmp      → ProfileFeatureSetKind::TonalPulse
ProfileInspectionRulesKind::FreqAmp → ProfileInspectionRulesKind::TonalPulse
```

Apply equivalent replacements for all profile-related enums and config labels.

---

## Profile Field Cleanup: signalEmitter / signalDetector

Current architectural finding:

```text
profile.signalEmitter and profile.signalDetector may be decorative / duplicated if actual detector wiring is hardcoded inside the occurrence source classes.
```

Use this rule:

```text
OccurrenceSource owns its source-specific detector path.

AmpOccurrenceSource uses scalar transient mechanics.
FrequencyOccurrenceSource uses FrequencyMatchDetector.

The profile should not carry a separate global signalDetector field unless detector choice is genuinely independent from source choice.
```

During the rename pass:

```text
- Rename these fields if they remain active and are needed for compile.
- If they are only decorative/log metadata, remove them from active profile config/logging.
- Do not add a new global occurrenceDetector field.
- Do not implement per-source config in this pass.
```

Preferred current documentation wording:

```text
Current implementation:
OccurrenceSource classes own detector wiring.

Future profile cleanup may replace decorative emitter/detector fields with per-source OccurrenceSourceConfig.
```

Do not change runtime logic in this pass.

---

## Files / Folders

Rename profile-specific files if present:

```text
FreqAmpProfile.*       → TonalPulseProfile.*
FreqAmpPatternRules.*  → TonalPulsePatternRules.*
FreqAmpConfig.*        → TonalPulseConfig.*
```

Only rename files that are specifically profile-named.

Do not rename generic frequency/amp detector files unless they are actually profile-specific.

Do not rename these if they are generic components:

```text
FrequencyMatchDetector
FrequencyBandStreamExtractor
AmpTransientDetector
FrequencyWindowProbe
```

Those are not profile names.

---

## Logs / Commands / Parameters

Update labels where they identify the profile:

```text
profile=FreqAmp       → profile=TonalPulse
PROFILE FreqAmp       → PROFILE TonalPulse
mode=freqamp          → mode=tonalpulse
```

If serial commands accept profile names, update accepted string:

```text
freqamp → tonalpulse
```

If backward compatibility is not required, remove old `freqamp` input.

If backward compatibility is kept temporarily, mark it explicitly:

```cpp
// Temporary alias for old serial profile name. Remove after migration.
```

Preferred: no compatibility alias unless needed for immediate usability.

---

## Docs

Update active docs:

```text
docs/myspec.md
detection roadmap docs
current-pass.md
README or architecture docs
analyzer docs
```

Replace profile references:

```text
FreqAmp profile       → TonalPulse profile
FreqAmp path          → TonalPulse path
FreqAmp strategy      → TonalPulse strategy
FreqAmp PatternRules  → TonalPulse PatternRules
```

Do not rewrite generic text about frequency + amplitude evidence unless it specifically names the profile.

---

## Part B Post-Rename Grep Checks

After renaming, run:

```bash
grep -R "FreqAmp\|freqAmp\|freqamp\|FREQ_AMP\|FREQAMP" -n src include docs test 2>/dev/null
```

Active code and active docs should have no `FreqAmp` references.

Allowed only in archived historical docs if clearly marked archived.

Confirm target name appears:

```bash
grep -R "TonalPulse\|tonalPulse\|tonalpulse\|TONAL_PULSE" -n src include docs test 2>/dev/null
```

Confirm no accidental `RonalPulse` remains:

```bash
grep -R "RonalPulse\|ronalPulse\|ronalpulse\|RONAL_PULSE" -n src include docs test 2>/dev/null
```

If `RonalPulse` appears, inspect and fix unless intentionally present in archived historical docs.

---

# Build / Verification

After both renames:

```bash
pio run
```

If the project has specific environments, use the normal build command for this repo.

Also inspect compile-time mode names that may be affected:

```text
ANALYZER_MODE
EMITTER_MODE
default Resonant mode
```

No runtime behavior should change.

---

# Final Acceptance Criteria

The pass is successful when:

```text
- Active code uses Occurrence terminology for bounded source-level detections.
- Active docs use Occurrence terminology in the detection flow.
- Continuous audio signal terminology still uses Signal where appropriate.
- PatternCandidate remains PatternCandidate.
- PatternResult remains PatternResult.
- FreqAmp is fully renamed to TonalPulse.
- RonalPulse does not remain in active code/docs.
- Generic frequency / amplitude component names are not renamed unless profile-specific.
- Decorative global signalDetector/signalEmitter fields are removed or explicitly justified if still present.
- Build succeeds.
- No detector logic, thresholds, PatternRules behavior, Analyzer semantics, or Behavior decisions changed.
```

---

# Important Concept Summary

Use this final conceptual vocabulary:

```text
AudioSignal / FeatureStream:
    continuous audio-derived material

Occurrence:
    bounded source-level acoustic happening from one evidence path

InspectedOccurrence:
    occurrence plus inspection decision / facts

PatternCandidate:
    assembled pattern-level candidate

PatternRules:
    pattern interpretation

PatternResult:
    semantic result consumed by Behavior / Analyzer
```

Profile naming:

```text
TonalPulse:
    current profile formerly named FreqAmp
```

Current implementation contract:

```text
OccurrenceSource classes own their detector wiring.
Profile-level detector kind should not be a separate global field unless detector choice is genuinely independent from source choice.
```
