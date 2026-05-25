# ResonantNode Cleanup Instruction — Post-Bugfix / Docs+Naming Pass

Scope: `src/` and active `docs/` after the latest bugfix, commenting, and rename pass.  
Project: ResonantNode / Resonanzraum Detection Refactor.  
Intent: finish half-transitions, remove remaining roadmap drift, and align code/help/docs with the current stable runtime.

This pass is **not** a new architecture expansion. It is a cleanup and alignment pass.

## Baseline decisions

Use these as hard constraints for this pass.

```text
D16 Chirp remains selectable only as explicitly experimental.
    Use `profile=chirp_experimental` / `ChirpExperimental`.
    Do not expose or accept stable/plain `profile=chirp`.

D17 AmpStateProfile is roadmap/proof-profile only.
    Do not mention it in normal manual/help.
    Do not add command parsing or selectable code now.

D18 Remove the `RB DETECT` alias entirely.
    Only `RB STATUS` remains.

D19 AMP diagnostics are Analyzer-only for now.
    Remove/disable AMP diagnostic probe from normal RB/Node exposure.
    Keep AMP diagnostics in Analyzer / SEQ_EXPLAIN / AMPDIAG.

D20 Clean Behavior transient wording now.
    Remove old boolean transient-update overload if unused.
    Rename Behavior internals from transient-oriented names to pattern/heard wording.

D21 Apply targeted rollover hardening now.
    Use timing helpers for key Behavior/runtime timing checks.
    Do not blindly rewrite the whole repo.

D22 Split DetectionRuntime reset now.
    resetState() clears runtime state only.
    applyProfile/configure owns profile/config fields.

D23 Update active docs to current Occurrence vocabulary now.
    Archive docs may keep old Signal wording.

D24 Global spelling pass: Occurrence, not Occurence.
    Apply to code, docs, comments, logs, filenames where relevant.

D25 Replace docs/current-pass.md with this next active cleanup pass.
    Archive old current-pass content first.

D26 Leave the engineer-rules filename typo for now.
    Do not rename `docs/engeinerr_ruels.md` in this pass.

D27 Leave archive docs untouched.
    Do not edit large historical archive docs.

D28 Add a separate active implementation-status document.
    Do not bury it in the main spec/current-pass.

D29 Manual may mention ChirpExperimental only in a clearly separate developer/experimental section.
    Normal quickstart/stable workflow shows TonalPulse only.

D30 No plain `profile=chirp` alias.
    If entered, return error/help suggesting `profile=chirp_experimental`.
```

---

## Global rules for this pass

### Do

```text
- Make code/help/docs tell the same truth.
- Keep TonalPulse as the only stable active profile.
- Keep ChirpExperimental selectable only with explicit experimental naming.
- Keep AmpState roadmap-only.
- Make diagnostic paths visibly diagnostic.
- Remove compatibility aliases that preserve old mental models.
- Use Occurrence vocabulary in active code/docs.
- Keep changes boring and local.
```

### Do not

```text
- Do not implement AmpStateProfile now.
- Do not implement full ChirpProfile / multi-pulse PatternRules now.
- Do not create generic rule engines, dynamic registries, schedulers, or broad BehaviorRuntime framework.
- Do not rename archive docs or clean the whole archive.
- Do not keep old and new command names side by side.
- Do not keep `profile=chirp` as a hidden alias.
- Do not expose AMP diagnostic in RB as if it were active detection.
```

---

# Pass order

## Pass 1 — Profile exposure cleanup

Status: DONE

### Goal

Make profile exposure match actual implementation status.

Stable:

```text
TonalPulse
```

Selectable experimental:

```text
ChirpExperimental
```

Roadmap only:

```text
AmpState
```

### Required changes

1. Rename command value for experimental chirp:

```text
profile=chirp_experimental
```

2. Do not accept:

```text
profile=chirp
```

3. If user enters `profile=chirp`, return an error/help message such as:

```text
Unknown profile `chirp`. Use `profile=chirp_experimental` for the experimental proof profile, or `profile=tonalpulse` for stable runtime.
```

4. Update enum/string/log naming if needed:

```text
Chirp -> ChirpExperimental
profileName = "ChirpExperimental"
command token = "chirp_experimental"
```

5. Help output:

Stable help should advertise:

```text
profile=tonalpulse
```

Experimental/developer help may mention:

```text
profile=chirp_experimental   experimental / proof profile / not stable
```

6. Do not add AmpState command parsing.

### Files likely involved

```text
src/detection/DetectionProfile.h/.cpp
src/modes/resonant/node.cpp / node_commands.cpp / help functions
src/modes/analyzer/AnalyzerCommands.cpp
src/modes/analyzer/AnalyzerReporting.cpp if profile names are printed
active docs/manual/help files
```

### Acceptance checks

```text
RB PROFILE name=tonalpulse               -> works
RB PROFILE name=chirp                    -> rejected, suggests chirp_experimental
RB PROFILE name=chirp_experimental       -> works, logs experimental name
SEQ ... profile=tonalpulse               -> works
SEQ ... profile=chirp                    -> rejected
SEQ ... profile=chirp_experimental       -> works, clearly experimental
Normal help shows TonalPulse as stable.
Experimental help/manual section may show ChirpExperimental.
AmpState does not appear in normal help/manual.
```

---

## Pass 2 — Remove `RB DETECT` alias

Status: DONE

### Goal

Remove old command vocabulary.

### Required changes

1. Remove parser support for:

```text
RB DETECT
```

2. Keep only:

```text
RB STATUS
```

3. Remove `RB DETECT` from comments, help, tests, and active docs.

4. If old notes mention `RB DETECT`, leave archive untouched, but do not mention it in active docs.

### Acceptance checks

```text
RB STATUS -> works
RB DETECT -> unknown command / helpful error
help/manual mention only RB STATUS
```

---

## Pass 3 — AMP diagnostics Analyzer-only

Status: DONE

### Goal

AMP diagnostics must not appear in RB/Node as active detection or normal status.

### Required changes

1. Remove or disable Node/RB-side `AmpDiagnosticProbe` member if it is not needed for normal runtime.

2. Remove RB status/log output such as:

```text
RB det mode=AMP
RB ampdiag ...
```

unless there is a deliberately separate RB AMPDIAG command. For this pass, prefer Analyzer-only.

3. Keep AMP diagnostics available in:

```text
Analyzer
SEQ_EXPLAIN
AMPDIAG
```

4. Normal RB runtime should be about:

```text
TonalPulse profile
PatternResult
FieldState
Behavior
Output
```

5. Do not let AMP diagnostic influence:

```text
DetectionRuntime truth
PatternResult
FieldState
Behavior
Analyzer classification
self-suppression
idle logic
response logic
```

### Files likely involved

```text
src/modes/resonant/node.*
src/modes/resonant/node_debug.cpp
src/modes/resonant/node_commands.cpp
src/modes/analyzer/*
src/detection/detectors/AmpDiagnosticProbe.*
```

### Acceptance checks

```text
RB STATUS does not print AMP diagnostic as active detector mode.
RB logs do not say det mode=AMP.
Analyzer SEQ_EXPLAIN / AMPDIAG still show AMP diagnostic facts.
No RB behavior path reads AmpDiagnosticProbe.
```

---

## Pass 4 — Behavior transient wording cleanup

Status: DONE

### Goal

Behavior should no longer expose old transient vocabulary. Behavior consumes PatternResult / heard pattern events.

### Required changes

1. Remove old boolean overload if unused:

```cpp
update(bool transientDetected, float transientStrength, unsigned long now)
```

2. Rename behavior internals:

```text
waitAfterTransientMs    -> waitAfterPatternMs or waitAfterHeardMs
pendingTransient*       -> pendingPattern* or pendingHeardPattern*
lastTransientMs         -> lastHeardPatternMs
transientDetected       -> patternDetected / validPatternHeard
transientStrength       -> patternStrength / heardPatternStrength
```

3. Prefer `heard` wording where the behavior cares about the fact that something was heard, not the low-level detection type:

```text
waitAfterHeardMs
lastHeardPatternMs
```

4. Keep behavior logic unchanged unless required by rename.

5. Do not start broad BehaviorRuntime/Profile/OutputDispatcher refactor.

### Files likely involved

```text
src/behavior/ResonantBehavior.h
src/behavior/ResonantBehavior.cpp
src/behavior/BehaviorProfile.h/.cpp if names appear there
src/modes/resonant/node.cpp if it calls old names
active docs/manual if behavior params are documented
```

### Acceptance checks

```text
No public Behavior API uses transient wording.
No internal behavior member uses pendingTransient / waitAfterTransient / lastTransient.
Behavior still consumes PatternResult + FieldState.
Response behavior remains recognizable.
```

---

## Pass 5 — Targeted rollover hardening

Status: DONE

### Goal

Use rollover-safe timing helpers for the important runtime deadlines.

### Existing principle

```text
Runtime timestamps are uint32_t / unsigned long milliseconds.
Elapsed time uses unsigned subtraction.
Window classification uses dt from a known anchor.
```

### Required changes

1. Use existing `TimingUtils` helpers or add small helpers if missing:

```cpp
elapsedSince(now, then, durationMs)
beforeDeadline(now, deadlineMs)
atOrAfter(now, deadlineMs)
```

2. Patch targeted areas only:

```text
Behavior wait deadline
Behavior suppression deadline
Behavior detection suppression deadline
Behavior refractory timing
Idle timing if present
Duplicate-risk elapsed checks
Analyzer SEQ window classification if still using absolute comparisons
```

3. Do not rewrite every comparison in the repo blindly.

4. For SEQ windows, prefer:

```cpp
uint32_t dt = eventMs - expectedTriggerMs;
bool inWindow = dt >= windowStartOffsetMs && dt <= windowEndOffsetMs;
```

5. Avoid:

```cpp
if (now > start + duration)
if (eventMs >= windowStartMs && eventMs <= windowEndMs)
if (acceptedMs > lastAcceptedMs ? acceptedMs - lastAcceptedMs : 0)
```

### Files likely involved

```text
src/util/TimingUtils.h
src/behavior/ResonantBehavior.cpp
src/detection/inspector/OccurrenceInspector.cpp or equivalent duplicate-risk logic
src/modes/analyzer/AnalyzerSequenceSession.cpp
src/modes/resonant/node.cpp
```

### Acceptance checks

```text
Behavior deadlines use timing helpers.
Duplicate-risk elapsed uses unsigned subtraction.
Analyzer windows classify by dt from expected trigger where applicable.
No logic changes except rollover-safe comparison.
```

---

## Pass 6 — Split DetectionRuntime reset

Status: DONE

### Goal

Separate runtime state reset from profile/config reset.

### Required changes

1. Replace ambiguous `reset()` behavior with explicit functions:

```cpp
resetState();     // runtime state only
applyProfile(...); // profile/config only
```

or keep `reset()` only if it clearly means state-only.

2. `resetState()` may clear:

```text
result queues
field tracker state
occurrence source state
detector/source runtime state
inspector transient state if any
pattern assembler transient state
pattern rules transient state if any
```

3. `resetState()` must not reset:

```text
active profile name
occurrence source kind
inspection rules kind
PatternRulesConfig
FieldStateConfig
BehaviorGateConfig
thresholds / gates
```

4. Profile switch should explicitly call:

```cpp
applyProfile(profile);
resetState(); // only if desired
```

5. SEQ start/runtime clear should call:

```cpp
resetState();
```

not a function that silently reverts to defaults.

### Files likely involved

```text
src/detection/DetectionRuntime.h
src/detection/DetectionRuntime.cpp
src/modes/resonant/node.cpp
src/modes/analyzer/AnalyzerApp.cpp
src/modes/analyzer/AnalyzerSequenceSession.cpp
```

### Acceptance checks

```text
SEQ reset does not revert active profile/config.
RB profile switch is explicit and logged.
DetectionRuntime profileName does not become "unknown" after normal state reset.
```

---

## Pass 7 — Active docs: Occurrence vocabulary + spelling

Status: DONE

### Goal

Active docs should match current code vocabulary.

### Required changes

1. Update active docs from old Signal terminology to current Occurrence terminology.

Replace in active docs where appropriate:

```text
SignalCandidate       -> Occurrence
SignalEmitter         -> OccurrenceSource
SignalDetector        -> Occurrence detector / source-specific detector where applicable
SignalInspector       -> OccurrenceInspector
InspectedSignal       -> InspectedOccurrence
Signal layer          -> Occurrence layer
```

2. Archive docs may keep old terms.

3. Add note only if needed:

```text
Older archive docs use SignalCandidate / SignalEmitter naming. Current active code uses Occurrence / OccurrenceSource.
```

4. Global spelling pass:

```text
Occurence -> Occurrence
```

Apply to:

```text
active docs
code symbols if any are misspelled
comments
logs
help text
filenames if relevant
```

5. Do not edit large archive docs in this pass.

### Files likely involved

```text
docs/myspec.md
docs/feature-roadmaps/detection-roadmap-v0.4-clean-gate-profile-composition.md
docs/notes_manual.md
docs/current-pass.md
src/** comments/logs if any misspell Occurrence
```

### Acceptance checks

```text
Active docs use Occurrence vocabulary.
No active doc says SignalCandidate as current code term.
No active doc/code has misspelled Occurence.
Archive docs untouched.
```

---

## Pass 8 — Replace `docs/current-pass.md`

Status: PENDING (kept active per user instruction)

### Goal

Make `docs/current-pass.md` describe the actual next active pass.

### Required changes

1. Archive old `current-pass.md` first:

```text
docs/archive/refactors/rename-signal-to-occurrence-tonalpulse.md
```

2. Create new `docs/current-pass.md` with this cleanup pass:

```text
Post-bugfix cleanup pass:
- ChirpExperimental exposure
- AmpState roadmap-only
- remove RB DETECT
- AMPDIAG Analyzer-only
- Behavior transient wording cleanup
- targeted rollover hardening
- split DetectionRuntime reset
- active docs Occurrence vocabulary
- Occurrence spelling pass
- implementation-status doc
```

3. Do not rename `docs/engeinerr_ruels.md` in this pass.

4. Do not add archive README in this pass.

### Acceptance checks

```text
docs/current-pass.md describes current cleanup pass.
Old rename pass content is archived.
Engineer-rules filename unchanged.
Archive docs otherwise untouched.
```

---

## Pass 9 — Add separate implementation-status doc

Status: DONE

### Goal

Add a short active status document to prevent stable/experimental/roadmap confusion.

### Required file

```text
docs/implementation-status.md
```

### Suggested content

```md
# ResonantNode Implementation Status

Status vocabulary:

- stable active
- selectable experimental
- roadmap only
- landed
- deferred

| Item | Status | Notes |
|---|---|---|
| TonalPulseProfile | stable active | Main runtime profile. |
| ChirpExperimental | selectable experimental | Proof profile, not stable normal runtime. Select with `profile=chirp_experimental`. |
| AmpStateProfile | roadmap only | Do not expose in help/manual/commands yet. |
| Occurrence rename | landed | Active code/docs use Occurrence vocabulary. |
| Analyzer valid gate | landed | Analyzer hit truth is PatternResult.valid. |
| AMP diagnostics in RB | removed / Analyzer-only | AMPDIAG belongs to Analyzer / SEQ_EXPLAIN. |
| Behavior transient wording cleanup | current pass | Rename to pattern/heard terms. |
| DetectionRuntime reset split | current pass | resetState vs applyProfile. |
| BehaviorRuntime | deferred | Future behavior architecture work. |
| OutputDispatcher | deferred | Future behavior/output separation. |
| Full Chirp PatternRules | roadmap only | Not part of stable runtime. |
| AmpState runtime | roadmap only | Not implemented now. |
```

### Acceptance checks

```text
Implementation-status doc exists.
It distinguishes stable active / selectable experimental / roadmap only / deferred.
It does not claim AmpState is implemented.
It does not claim ChirpExperimental is stable.
```

---

## Pass 10 — Manual/help update

Status: DONE

### Goal

Make the normal manual stable-first, with experimental profile separated.

### Required changes

1. Normal quickstart shows only:

```text
profile=tonalpulse
```

2. Add a separate section:

```text
Developer / Experimental profiles
```

3. In that section only, document:

```text
profile=chirp_experimental
```

with wording:

```text
experimental / proof profile / not stable / not normal use
```

4. Do not mention AmpState in normal manual/help.

5. Do not mention `profile=chirp`.

### Acceptance checks

```text
Normal manual workflow uses TonalPulse only.
ChirpExperimental appears only in experimental section.
AmpState does not appear in normal help/manual.
profile=chirp does not appear except maybe as an invalid example.
```

---

# Final acceptance checklist

Run these checks after all passes.

## Command behavior

```text
RB STATUS                                    works
RB DETECT                                    rejected / unknown
RB PROFILE name=tonalpulse                   works
RB PROFILE name=chirp                        rejected, suggests chirp_experimental
RB PROFILE name=chirp_experimental           works, experimental label visible
SEQ ... profile=tonalpulse                   works
SEQ ... profile=chirp                        rejected
SEQ ... profile=chirp_experimental           works, experimental label visible
```

## Runtime architecture

```text
TonalPulse remains the only stable active profile.
ChirpExperimental is clearly experimental.
AmpState is roadmap only.
RB/Node does not expose AMP diagnostic as active detection.
Behavior no longer exposes transient-oriented API/names.
DetectionRuntime reset does not silently reset profile/config.
```

## Timing

```text
Behavior deadlines use rollover-safe helpers.
Duplicate-risk elapsed uses unsigned subtraction.
SEQ windows use dt-from-anchor where applicable.
No broad risky timing rewrite was introduced.
```

## Docs

```text
Active docs use Occurrence vocabulary.
Occurrence spelling is correct.
docs/current-pass.md describes this pass.
Old current-pass content is archived.
docs/implementation-status.md exists.
Normal manual shows TonalPulse only.
Experimental section may mention ChirpExperimental.
Archive docs untouched.
```

---

# Stop conditions

Stop and report if:

```text
- profile=chirp must be kept for compatibility; this contradicts D30.
- AmpState appears required by code; this contradicts D17.
- Behavior boolean transient update is still used by Node or Analyzer.
- DetectionRuntime reset split requires broad invasive changes.
- Occurrence spelling change affects public command compatibility.
```

In those cases, report the conflict instead of adding compatibility sediment.
