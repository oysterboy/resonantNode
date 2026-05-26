# Current Pass — Detection Naming Cleanup + 5-Node Status Baseline

Scope: small MVP for the next real use case.

Next use case:

```text
test current TonalPulse detection and behavior variations on 5 nodes
```

This pass should make the current code/logs readable and make node identity/config visible before physical tests.

---

## MVP Guardrail

This pass is small, but it must not be disposable.

Do not fix naming or status by adding temporary compatibility layers, duplicate APIs, or node-level hacks that immediately need removal.

Preferred pattern:

```text
rename cleanly
keep ownership in the right module
expose already-owned values
compile
```

Avoid:

```text
old/new aliases
random status constants in node.cpp
duplicated profile names
manual lists that will fight module ownership
```

---

## Goal

1. Rename bounded source-level detection objects from `Signal*` terminology to `Occurrence*`.
2. Rename the current `FreqAmp` profile to `TonalPulse`.
3. Add/clean a minimal status baseline for 5-node testing.
4. Do not change runtime behavior.

---

## Part A — Rename Detection Terms

Rename active code and active docs:

```text
SignalCandidate        → Occurrence
SignalEmitter          → OccurrenceSource
SignalInspector        → OccurrenceInspector
InspectedSignal        → InspectedOccurrence
AmpSignalEmitter       → AmpOccurrenceSource
FrequencySignalEmitter → FrequencyOccurrenceSource
FreqAmp                → TonalPulse
```

Keep:

```text
AudioSignal
FeatureStream
PatternCandidate
PatternRules
PatternResult
```

because these names remain architecturally useful.

---

## Part B — Preflight for TonalPulse

Before renaming `FreqAmp`, search for existing target/spelling conflicts:

```bash
grep -R "TonalPulse\|tonalPulse\|tonalpulse\|TONAL_PULSE\|TonalPulse\|ronalPulse\|ronalpulse\|RONAL_PULSE" -n src include docs test 2>/dev/null
```

Rules:

```text
TonalPulse in active files = stale/wrong spelling; rename to TonalPulse.
Existing TonalPulse = inspect first; do not create a second competing concept.
```

---

## Part C — 5-Node Status Baseline

Add or clean a status/log output that helps compare 5 nodes.

Minimum useful status fields:

```text
firmware version or build label if available
node id if available
active detection profile
active behavior mode / behavior enabled flag if available
key TonalPulse detection params if hardcoded in config/defaults
key behavior params if hardcoded in config/defaults
output busy/status if already available
```

Do not build ParamRegistry for this pass.

Do not implement persistence.

If a field does not exist yet, do not invent a large subsystem for it. Add only cheap status exposure for already-existing values.

Keep ownership correct:

```text
detection params should come from detection/profile config
behavior params should come from behavior config
output status should come from SoundOutput if available
```

Do not create a random node-level status mirror unless no owner exists yet and the field is explicitly marked transitional.

---

## Part D — Decorative Profile Fields

Known issue:

```text
profile.signalEmitter / profile.signalDetector may be decorative if actual detector wiring is hardcoded inside occurrence source classes.
```

For this pass:

```text
- do not implement a new config system
- do not add a new global occurrenceDetector field
- if these fields are only decorative/log metadata, remove them from active profile config/logging
- if needed for compile, rename consistently and mark as future cleanup
```

Current contract:

```text
OccurrenceSource classes own their detector wiring.
Profile-level detector kind should not be a separate global field unless detector choice is genuinely independent from source choice.
```

---

## Do Not

```text
- change detector logic
- change PatternRules
- change thresholds
- change behavior decisions
- change Analyzer classification semantics
- change output timing
- add OccurrenceSourceConfig
- add CandidateCorrelator
- add new profiles
- add ParamRegistry
- add PARAM SET
- add SAVE / LOAD
- keep compatibility aliases unless absolutely required for compilation
```

If a temporary alias is required for compilation, add a TODO and remove it in the same pass or next cleanup pass.

---

## Verification

After renaming, run:

```bash
grep -R "SignalCandidate\|InspectedSignal\|SignalInspector\|SignalEmitter\|AmpSignalEmitter\|FrequencySignalEmitter\|SignalSource" -n src include docs test 2>/dev/null
```

Run:

```bash
grep -R "FreqAmp\|freqAmp\|freqamp\|FREQ_AMP\|FREQAMP" -n src include docs test 2>/dev/null
```

Run:

```bash
grep -R "TonalPulse\|ronalPulse\|ronalpulse\|RONAL_PULSE" -n src include docs test 2>/dev/null
```

Allowed remaining matches only in clearly archived historical docs.

Then build:

```bash
pio run
```

---

## Success Criteria

```text
- Active code uses Occurrence terminology for bounded source-level detections.
- Active docs use Occurrence terminology in the detection flow.
- Continuous audio signal terminology still uses Signal.
- FreqAmp is renamed to TonalPulse.
- TonalPulse does not remain in active code/docs.
- PatternCandidate and PatternResult remain unchanged.
- STATUS/log output is sufficient to identify profile/mode/key hardcoded params across 5 nodes.
- Status values come from the appropriate module/profile owner where possible.
- Build succeeds.
- No detector logic, thresholds, PatternRules, Analyzer semantics, or Behavior decisions changed.
```
