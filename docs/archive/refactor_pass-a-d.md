# Detection Roadmap v0.3 — Codex Pass Notes A–E

Status: reconstructed from chat notes  
Scope: Codex implementation instructions for Sections A–E  
Note: This file preserves the pass notes as closely as possible from the discussion.

---

# Section A — Immediate Cleanup / Stabilization

## A. Immediate cleanup / stabilization

### 1. Make `DetectionRuntime` the main Resonant detection path

Ensure the active Resonant behavior path receives detection output from `DetectionRuntime`, not from legacy detector-side shortcuts.

Codex notes:
- Trace the current runtime path from node loop to behavior input.
- Prefer `DetectionRuntime` as the single orchestration point for detection.
- Behavior should receive `PatternResult` objects, not raw signal/candidate objects.
- Do not delete legacy paths blindly; isolate or mark them as fallback/reference if still needed.
- Keep Analyzer/test behavior working.

Success check:
- There is one clear main path: `DetectionRuntime → PatternResult → Behavior`.
- Legacy candidate handling is not the default behavior trigger.

---

### 2. Reduce or isolate legacy AMP candidate handling

Move old AMP candidate handling out of the main behavior path or wrap it behind the newer signal pipeline.

Codex notes:
- Find remaining uses of `AmpCandidateBuilder`, `_audioOnsetDetector`, or equivalent legacy AMP candidate paths.
- Keep AMP-first as a reference baseline, but do not let it bypass `SignalCandidate → SignalInspector → PatternAssembler → PatternRules`.
- If old AMP path is still needed for Analyzer/SEQ comparison, name it clearly as legacy/reference.
- Avoid duplicating AMP validation logic in both old and new paths.

Success check:
- AMP can still produce candidates/results, but through the shared detection stages or clearly isolated reference code.
- No hidden AMP path directly drives behavior.

---

### 3. Rename `FrequencyTransient` to `FrequencyMatch`

Align naming with Roadmap v0.3: the frequency path is match-based, not necessarily a generic scalar transient.

Codex notes:
- Rename enum values, source labels, log labels, and comments where appropriate.
- Preferred naming:
  - `SignalKind::FrequencyMatch`
  - `SignalSource::TargetFrequency` or equivalent
  - `FrequencyMatchDetector`
- Avoid names that imply the frequency path must use `TransientDetector`.
- Keep backward-compatible log parsing only if existing tests require it.

Success check:
- Runtime/log terminology says `FrequencyMatch` rather than `FrequencyTransient`.
- No semantic confusion between `TransientDetector` and `FrequencyMatchDetector`.

---

### 4. Keep `FrequencyMatchDetector` contained at signal-detection level

Preserve the working frequency-first behavior, but keep its responsibility limited to signal candidate creation.

Codex notes:
- `FrequencyMatchDetector` may own frequency-specific candidate lifecycle:
  - matched window
  - last matched time
  - score / contrast evidence selection
  - release / refractory behavior
- It must not own:
  - AMP locality inspection
  - PatternAssembly
  - PatternRules
  - Behavior decisions
- Output should normalize into `SignalCandidate`.

Success check:
- `FrequencyMatchDetector` emits or supports creation of `SignalCandidate`.
- Any near/far/locality meaning is handled later by `SignalInspector` / `PatternRules`, not inside the detector.

---

### 5. Clarify `AmpSignalEmitter` and `FrequencySignalEmitter` as emitters, not mini-pipelines

Keep emitter classes thin: they connect input features/detectors to `SignalCandidate` output.

Codex notes:
- `AmpSignalEmitter` may wrap `ScalarSignalEmitter` / `TransientDetector`.
- `FrequencySignalEmitter` may wrap `FrequencyMatchDetector`.
- Emitters may choose source tags and detector params.
- Emitters should not inspect candidates, assemble patterns, apply pattern rules, or call behavior.
- If emitter files contain extra logic, move that logic to `SignalInspector`, `PatternAssembler`, or `PatternRules`.

Success check:
- Emitters answer only: “Which detector on which input produces which `SignalCandidate`?”
- No emitter emits `PatternResult`.

---

### 6. Keep `PatternAssembler` trivial but explicit

Keep the current one-signal-to-one-pattern assembly for now, but make the architectural seam clear.

Codex notes:
- Initial assembler may do:
  - accepted `InspectedSignal`
  - → pulse-like `PatternCandidate`
- This is acceptable as `TrivialPatternAssembler` behavior.
- Do not put signal inspection or pattern meaning into the assembler.
- Prepare the interface so later chirp grouping can use multiple `InspectedSignals`.
- Document that one `InspectedSignal` may later belong to multiple `PatternCandidates`.

Success check:
- `SignalInspector` does not emit `PatternCandidate` directly.
- `PatternAssembler` is the only stage creating `PatternCandidate`, even if currently trivial.

---

### 7. Remove duplicated signal-level validation from `PatternRules`

Keep `PatternRules` focused on pattern meaning, not low-level signal acceptance.

Codex notes:
- Review `PatternRules` for repeated frequency/AMP acceptance checks that belong in `SignalInspector`.
- Signal-level validity should be represented in `InspectedSignal`.
- PatternRules may use inspected facts, but should not re-run detector-like logic.
- PatternRules decide meanings such as:
  - `tonalPulse`
  - `nearTonalPulse`
  - `farTonalPulse`
  - `validChirp`
  - `residual`
  - `rejected`

Success check:
- `SignalInspector` handles accept/reject/annotate.
- `PatternRules` handles interpretation of `PatternCandidate`.
- No detector lifecycle logic appears in `PatternRules`.

---

### 8. Clean Analyzer / debug logs around signal → inspected signal → pattern result

Make logs reflect the new pipeline stages clearly enough to debug misses, late hits, duplicates, and locality.

Codex notes:
- Prefer stage-specific log prefixes or fields:
  - `SIGNAL`
  - `INSPECTED`
  - `PATTERN_CANDIDATE`
  - `PATTERN_RESULT`
  - `FIELD_STATE`
- Include source/kind consistently:
  - amp transient
  - frequency match
  - broadband later
- Log rejection reasons at the stage where rejection happens.
- Avoid duplicate noisy logs from multiple layers saying the same thing.
- Preserve useful SEQ/analyzer outputs where possible.

Success check:
- A single event can be followed through:
  - `SignalCandidate`
  - `InspectedSignal`
  - `PatternCandidate`
  - `PatternResult`
- Rejections are explainable by stage and reason.

---

# Section B — Signal Layer Completion

# Codex Pass — Section B: Signal Layer Completion

## Goal

Complete the signal layer so all signal-level outputs are structurally consistent, explainable, and ready for later inspection, pattern assembly, and behavior separation.

This pass should **not** change the larger detection architecture.

Do not introduce `DetectionProfile` yet.

Do not rewrite `FrequencyMatchDetector`.

Do not move pattern meaning into the signal layer.

---

## Context

Section A is structurally landed but still has cleanup edges.

The current pipeline target is:

```text
SignalEmitter
→ SignalDetector
→ SignalCandidate
→ SignalInspector
→ InspectedSignal
→ PatternAssembler
→ PatternCandidate
→ PatternRules
→ PatternResult
```

Section B focuses only on the signal layer:

```text
SignalCandidate
→ SignalInspector
→ InspectedSignal
```

---

## Roadmap Section B items

9. Stabilize `SignalCandidate` structure  
10. Stabilize `InspectedSignal` structure  
11. Add signal acceptance / rejection reasons  
12. Add duration / strength / confidence fields  
13. Add duplicate-risk annotation  
14. Add source tags and detector provenance consistently  
15. Support multiple `SignalDetector` implementations under one signal layer

---

## 1. Stabilize `SignalCandidate`

### Target

`SignalCandidate` is the common low-level candidate emitted by all signal emitters.

It should represent:

```text
Something signal-like may have happened around t.
```

It must not represent:

```text
valid chirp
near tonal pulse
far tonal pulse
behavior trigger
```

### Required fields

Ensure `SignalCandidate` consistently supports:

```cpp
SignalSource source;
SignalKind kind;

uint32_t startMs;
uint32_t peakMs;
uint32_t endMs;

float strength;
float confidence;

SignalDetectorKind detectorKind;
```

Use existing field names if already present. Do not rename everything unless needed.

### Source / kind examples

Preferred:

```cpp
SignalSource::Amp
SignalSource::TargetFrequency
SignalSource::Broadband // later

SignalKind::AmpTransient
SignalKind::FrequencyMatch
SignalKind::BroadbandTransient // later
```

Avoid:

```cpp
FrequencyTransient
```

where the active frequency detector is now `FrequencyMatchDetector`.

---

## 2. Stabilize `InspectedSignal`

### Target

`InspectedSignal` is the result of signal-level inspection.

It may be:

```text
accepted
rejected
weak
duplicate-risk
annotated
```

It is still not a pattern.

It must not represent:

```text
valid chirp
noise burst
nearTonalPulse
PatternResult
```

### Required fields

Add or normalize fields like:

```cpp
SignalCandidate candidate;

bool accepted;
SignalRejectReason rejectReason;

float durationMs;
float strength;
float confidence;

AmpSupportClass ampSupport;
LocalityClass locality;

bool duplicateRisk;
float duplicateRiskScore;
```

If some fields are premature, add enums / placeholders and default them safely.

---

## 3. Add signal acceptance / rejection reasons

### Target

Rejections should be explainable at signal stage.

Create or stabilize:

```cpp
enum class SignalRejectReason {
  None,
  TooShort,
  TooLong,
  TooWeak,
  BelowThreshold,
  DuplicateRisk,
  Cooldown,
  MissingFrequencyEvidence,
  MissingAmpSupport,
  InvalidTiming,
  Unknown
};
```

Use only the reasons currently needed. Do not overfit all future cases.

### Rule

SignalInspector decides signal acceptance/rejection.

PatternRules should not rediscover basic signal acceptance.

---

## 4. Add duration / strength / confidence fields

### Target

Every inspected signal should expose basic comparable facts:

```text
duration
strength
confidence
```

These should be available regardless of whether the source is AMP or frequency.

Examples:

```cpp
durationMs = endMs - startMs;
strength = candidate.strength;
confidence = computed or defaulted;
```

Do not invent complex scoring yet.

Use conservative defaults:

```cpp
confidence = 1.0f for accepted known-good candidates
confidence = 0.0f for rejected candidates
```

or preserve existing confidence if already available.

---

## 5. Add duplicate-risk annotation

### Target

Duplicate risk belongs to signal inspection, not pattern meaning.

Add fields:

```cpp
bool duplicateRisk;
float duplicateRiskScore;
```

or equivalent.

Initial implementation can be simple:

```text
mark duplicateRisk if candidate occurs too soon after previous accepted signal from same source
```

Do not block all duplicates unless existing behavior already does so.

This pass may only annotate duplicate risk; later PatternRules or Behavior can decide how to use it.

---

## 6. Add source tags and detector provenance consistently

### Target

Every `SignalCandidate` / `InspectedSignal` should preserve:

```text
which source produced it
which detector produced it
which signal kind it represents
```

Example:

```cpp
source = SignalSource::TargetFrequency
kind = SignalKind::FrequencyMatch
detectorKind = SignalDetectorKind::FrequencyMatch
```

For AMP:

```cpp
source = SignalSource::Amp
kind = SignalKind::AmpTransient
detectorKind = SignalDetectorKind::Transient
```

This is important for logs, Analyzer, and later DetectionProfile work.

---

## 7. Support multiple SignalDetector implementations

### Target

The signal layer must not assume that every signal comes from `TransientDetector`.

Current implementations:

```text
TransientDetector
FrequencyMatchDetector
```

Future possible implementations:

```text
DipDetector
PlateauDetector
ThresholdCrossingDetector
```

Add or stabilize:

```cpp
enum class SignalDetectorKind {
  Transient,
  FrequencyMatch,
  Dip,
  Plateau,
  ThresholdCrossing,
  Unknown
};
```

Do not implement future detectors now.

Only make the type system ready.

---

## 8. Logging requirements

Update logs enough to inspect signal-stage correctness.

Preferred stage labels:

```text
SIGNAL
INSPECTED
```

A useful signal log should show:

```text
source
kind
detectorKind
startMs
peakMs
endMs
durationMs
strength
confidence
```

A useful inspected log should show:

```text
accepted
rejectReason
ampSupport
locality
duplicateRisk
confidence
```

Do not create noisy per-loop logs. Log candidate-level events only.

---

## 9. Boundaries to preserve

Do not let `SignalInspector` emit `PatternCandidate`.

Correct:

```text
SignalCandidate
→ SignalInspector
→ InspectedSignal
→ PatternAssembler
```

Do not let `SignalInspector` emit `PatternResult`.

Correct:

```text
PatternRules
→ PatternResult
```

Do not let Behavior consume `SignalCandidate` or `InspectedSignal`.

Correct:

```text
Behavior consumes PatternResult + FieldState
```

---

## 10. Success criteria

After this pass:

```text
SignalCandidate has stable source/kind/timing/strength/provenance fields.

InspectedSignal has stable acceptance/rejection/facts fields.

FrequencyMatch is used instead of FrequencyTransient where applicable.

SignalRejectReason exists and is used by SignalInspector.

Duration, strength, confidence are consistently available.

Duplicate risk can be annotated.

AMP and frequency candidates both use the same signal-layer structures.

TransientDetector and FrequencyMatchDetector both fit under SignalDetectorKind.

PatternAssembler remains the first stage that creates PatternCandidates.

PatternRules remain the first stage that creates PatternResults.
```

---

## 11. Do not do in this pass

Do not:

```text
introduce DetectionProfile
introduce external config
rewrite FrequencyMatchDetector
implement chirp grouping
implement white-noise detection
implement new detectors
move behavior logic
change output sound behavior unless required for compile fixes
```

This pass is signal-layer stabilization only.

---

# Section C — Frequency-First Refinement

# Codex Pass — Section C: Frequency-First Refinement

## Goal

Refine the frequency-first path so it remains reliable but regains physical/local neighbor sensitivity through AMP inspection.

Do **not** rewrite `FrequencyMatchDetector`.

Do **not** remove legacy AMP yet.

Do **not** introduce `DetectionProfile` yet.

This pass focuses on:

```text
FrequencyMatch SignalCandidate
→ SignalInspector adds AMP support / locality
→ InspectedSignal
→ PatternAssembler
→ PatternRules classify near/mid/far or weak-locality patterns
```

---

## Context

Current Roadmap v0.3 rules:

```text
Detector creates SignalCandidate.
Evaluator adds evidence.
PatternRules interpret PatternCandidate.
Behavior consumes PatternResult + FieldState.
```

`FrequencyMatchDetector` is allowed to own frequency-specific signal lifecycle:

```text
matched window
last matched time
score / contrast evidence selection
frequency-specific release
frequency-specific refractory
```

But it must not own:

```text
AMP locality inspection
PatternAssembly
PatternRules
Behavior decisions
```

Frequency-first currently works well, possibly too well across distance. The goal is not to weaken frequency detection. The goal is to add AMP/locality evidence after candidate creation.

---

## Roadmap Section C items

16. Add AMP locality inspection for frequency-first candidates  
17. Add `ampSupportClass`  
18. Add `localityClass`  
19. Separate frequency match confidence from physical locality  
20. Add near / mid / far interpretation in pattern rules  
21. Keep frequency lifecycle inside `FrequencyMatchDetector`  
22. Keep frequency evidence evaluation separate from frequency detection  

---

## 1. Add AMP locality inspection for frequency-first candidates

### Target

When a `SignalCandidate` comes from `FrequencyMatchDetector`, the `SignalInspector` should inspect the AMP envelope around the candidate window.

Correct:

```text
FrequencyMatchDetector
→ SignalCandidate(source = TargetFrequency, kind = FrequencyMatch)
→ SignalInspector
→ inspect amp evidence around candidate
→ InspectedSignal with amp/locality facts
```

Wrong:

```text
FrequencyMatchDetector
→ decides near/far
```

### Implementation guidance

Use available feature/audio history. Prefer existing AMP feature stream/history if available.

If no formal `FeatureHistory` API exists yet, use the current available AMP level/history mechanism without introducing a large abstraction.

Keep this pass practical.

Inspection window should be candidate-relative, for example:

```text
candidate start - small pre padding
→ candidate end + small post padding
```

or use existing timing fields.

Do not tune thresholds heavily in this pass. Add conservative defaults.

---

## 2. Add `ampSupportClass`

### Target

Add an enum or equivalent:

```cpp
enum class AmpSupportClass {
  None,
  Weak,
  Medium,
  Strong,
  Unknown
};
```

Store it on `InspectedSignal`.

Suggested semantics:

```text
Strong:
AMP clearly supports the frequency event.

Medium:
AMP support exists but is not dominant.

Weak:
AMP support is present but probably far / indirect.

None:
No meaningful AMP support found.

Unknown:
AMP evidence unavailable or not evaluated.
```

Use conservative thresholds.

Do not block frequency candidates based only on weak AMP support yet, unless existing behavior already requires it.

---

## 3. Add `localityClass`

### Target

Add an enum or equivalent:

```cpp
enum class LocalityClass {
  Near,
  Mid,
  Far,
  Unknown
};
```

Store it on `InspectedSignal`.

Initial mapping may be simple:

```text
AmpSupportClass::Strong → Near
AmpSupportClass::Medium → Mid
AmpSupportClass::Weak → Far
AmpSupportClass::None → Far or Unknown
AmpSupportClass::Unknown → Unknown
```

Keep mapping centralized in `SignalInspector` or a small helper.

Do not put this mapping in `FrequencyMatchDetector`.

---

## 4. Separate frequency match confidence from physical locality

### Target

`InspectedSignal` should distinguish:

```text
frequencyConfidence
```

from:

```text
ampSupportClass / localityClass
```

Reason:

```text
Frequency asks:
“Was the correct signal present?”

AMP/locality asks:
“Was it physically strong / local enough?”
```

Do not collapse both into one confidence field.

Suggested fields:

```cpp
float signalConfidence;      // general signal acceptance confidence
float frequencyConfidence;   // confidence in target frequency match
AmpSupportClass ampSupport;
LocalityClass locality;
```

If the structure already has only `confidence`, add `frequencyConfidence` only for frequency candidates, with default `0.0f` or `NAN` / optional if appropriate.

---

## 5. Add near / mid / far interpretation in PatternRules

### Target

`PatternRules` should interpret inspected locality facts into pattern results.

Possible `PatternResult` kinds:

```cpp
TonalPulseNear
TonalPulseMid
TonalPulseFar
TonalPulseUnknownLocality
```

or, if the existing result vocabulary is smaller, add fields instead:

```cpp
PatternResult.kind = TonalPulse
PatternResult.locality = Near / Mid / Far / Unknown
```

Prefer the least disruptive option.

### Boundary

PatternRules may use:

```text
InspectedSignal.localityClass
InspectedSignal.ampSupportClass
InspectedSignal.frequencyConfidence
```

PatternRules should not re-run AMP inspection.

PatternRules should not ask `FrequencyMatchDetector` for locality.

---

## 6. Keep frequency lifecycle inside `FrequencyMatchDetector`

### Target

Do not move frequency-specific lifecycle logic out during this pass.

Leave inside `FrequencyMatchDetector`:

```text
matched-window lifecycle
last matched time
release logic
frequency-specific refractory
score / contrast evidence selection
```

Only adjust it if required to expose clean candidate fields.

---

## 7. Keep frequency evidence evaluation separate from frequency detection

### Target

Preserve this naming/role split:

```text
FrequencyMatchDetector
= signal-level detector
= emits SignalCandidate

FrequencyEvidenceEvaluator
= inspection-level evaluator
= adds frequency facts to an existing signal
```

Do not introduce `FrequencyEvidenceDetector`.

If there is code that evaluates frequency inside SignalInspector, name it evaluator-style.

---

## 8. Logging requirements

Add or improve logs so frequency-first locality is visible.

Useful signal/inspection fields:

```text
source=TargetFrequency
kind=FrequencyMatch
freqConfidence=...
ampSupport=Strong/Medium/Weak/None/Unknown
locality=Near/Mid/Far/Unknown
accepted=true/false
rejectReason=...
```

Useful pattern fields:

```text
patternKind=TonalPulse
locality=Near/Mid/Far/Unknown
```

Avoid noisy per-loop logs. Log candidate-level events.

---

## 9. Success criteria

After this pass:

```text
FrequencyMatch candidates are still detected reliably.

SignalInspector adds AMP support/locality facts to frequency-first candidates.

AmpSupportClass exists and is visible in logs.

LocalityClass exists and is visible in logs.

Frequency confidence and AMP/locality are separate.

PatternRules can emit or annotate near/mid/far tonal pulse results.

FrequencyMatchDetector still does not own AMP locality, PatternAssembly, PatternRules, or Behavior.

Behavior still consumes PatternResult + FieldState only.

Legacy AMP remains isolated and not removed.
```

---

## 10. Do not do in this pass

Do not:

```text
remove legacy AMP
introduce DetectionProfile
rewrite FrequencyMatchDetector as TransientDetector
implement chirp grouping
implement white-noise detection
implement new SignalDetectors
move behavior logic
add external config
perform heavy threshold tuning
```

This pass is frequency-first locality refinement only.

---

# Section D — Inspection Mechanic

# Codex Pass — Section D: Inspection Mechanic

## Goal

Generalize the inspection layer so AMP-first, frequency-first, and later broadband/object chains use the same inspection mechanism.

This pass upgrades signal inspection from ad hoc / snapshot-style checks toward reusable:

```text
SignalCandidate
→ SignalInspector
→ InspectionRules
→ ScalarWindow / RawWindow
→ WindowEvaluators
→ InspectedSignal
```

Main target:

```text
Primary source changes.
Inspection mechanic stays the same.
```

Do **not** introduce `DetectionProfile` yet.

Do **not** remove legacy AMP yet.

Do **not** rewrite `FrequencyMatchDetector`.

---

## Roadmap Section D items

23. Generalize `SignalInspector`  
24. Introduce reusable `InspectionRule`  
25. Introduce window evaluators  
26. Use `ScalarWindow` from `FeatureHistory` as preferred inspection path  
27. Keep `RawWindow` from `AudioHistory` as fallback / advanced path  
28. Reuse the same inspection mechanic for AMP-first and frequency-first  
29. Add broadband / tonal-rejection inspection rules later  

---

## 1. Generalize `SignalInspector`

### Target

`SignalInspector` should become the common signal-level inspection stage.

It consumes:

```text
SignalCandidate
```

and emits:

```text
InspectedSignal
```

It may:

```text
accept
reject
annotate
add evidence
add locality
add confidence
add duplicate-risk
add rejection reason
```

But it must not create:

```text
PatternCandidate
PatternResult
Behavior decision
```

### Current target flow

```text
SignalCandidate
→ SignalInspector
→ InspectedSignal
→ PatternAssembler
→ PatternCandidate
→ PatternRules
→ PatternResult
```

---

## 2. Introduce reusable `InspectionRule`

### Target

Move source-specific inspection logic into small reusable rules.

Create or stabilize:

```cpp
class InspectionRule {
public:
  virtual bool appliesTo(const SignalCandidate& candidate) const = 0;
  virtual void inspect(
    const SignalCandidate& candidate,
    InspectionContext& context,
    InspectedSignal& out
  ) = 0;
};
```

Use the existing project style; exact interface may differ.

### Example rules

Start with practical rules only:

```text
AddBasicTimingFacts
AddAmpStats
AddAmpLocality
AddFrequencyFacts
AddDuplicateRisk
```

Later rules:

```text
AddTonalRejection
AddBroadbandStats
AddTailEnergy
AddAmbientContrast
```

### Boundary

`InspectionRule` may add facts to `InspectedSignal`.

It must not emit `PatternCandidate`.

It must not emit `PatternResult`.

---

## 3. Introduce `InspectionContext`

### Target

Rules need access to histories and evaluators without hardwiring global objects.

Create or use a small context object:

```cpp
struct InspectionContext {
  FeatureHistory* featureHistory;
  AudioHistory* audioHistory;
  uint32_t nowMs;
};
```

Include only what is already available.

Do not overbuild.

No dynamic plugin registry.

No external config.

---

## 4. Introduce `ScalarWindow` inspection path

### Target

Use `ScalarWindow` from `FeatureHistory` as the preferred normal inspection path.

Correct:

```text
SignalCandidate
→ request ScalarWindow(ampEnv, candidate-relative window)
→ evaluate amp peak / mean / support
→ InspectedSignal.ampSupportClass / localityClass
```

Avoid creating separate structures:

```text
AmpWindow
FreqWindow
NoiseWindow
```

Use one generic scalar window concept.

### Needed API shape

Something like:

```cpp
ScalarWindow FeatureHistory::getWindow(
  FeatureStreamId stream,
  uint32_t startMs,
  uint32_t endMs
);
```

or a project-compatible equivalent.

If `FeatureHistory` is not ready yet, introduce the smallest adapter needed.

---

## 5. Introduce `WindowSpec`

### Target

Inspection should request candidate-relative windows consistently.

Possible structure:

```cpp
struct WindowSpec {
  WindowAnchor anchor;   // CandidateStart, CandidatePeak, CandidateEnd
  int32_t beforeMs;
  int32_t afterMs;
};
```

Useful windows:

```text
candidateWindow:
start → end

earlyWindow:
start → start + N ms

preWindow:
start - N ms → start

postWindow:
end → end + N ms

tailWindow:
end → end + N ms
```

Do not implement every window if unnecessary.

For this pass, at minimum support:

```text
candidate-relative AMP window
```

for frequency-first locality.

---

## 6. Introduce basic `WindowEvaluator`s

### Target

Window math should be reusable.

Start with simple evaluators:

```text
BasicScalarStatsEvaluator
AmpSupportEvaluator
```

Possible outputs:

```text
min
max / peak
mean
first
last
rise
durationAboveThreshold
peakTime
```

For AMP locality, useful facts:

```text
ampPeak
ampMean
ampSupportClass
localityClass
```

Do not add median if expensive/unneeded.

Do not add complex scoring yet.

---

## 7. Replace snapshot-style AMP locality with retrospective AMP window

### Target

Pass C may have added AMP locality using a current/snapshot value.

This pass should upgrade it to:

```text
FrequencyMatch SignalCandidate
→ SignalInspector
→ request ScalarWindow(ampEnv, candidate-relative window)
→ evaluate amp support
→ InspectedSignal.ampSupportClass
→ InspectedSignal.localityClass
```

This is the key practical feature of Pass D.

### Rule

Do not compute locality inside `FrequencyMatchDetector`.

Do not compute locality inside `PatternRules`.

Correct owner:

```text
SignalInspector / InspectionRule
```

---

## 8. Keep `RawWindow` as fallback / advanced path

### Target

`RawWindow` from `AudioHistory` remains allowed for expensive or transitional evaluations.

Current / possible use:

```text
RawWindow
→ GoertzelRawWindowEvaluator
→ frequency facts
```

Use RawWindow when:

```text
feature is expensive
feature is experimental
feature is not yet promoted to FeatureStream
feature is only needed after a candidate exists
```

But preferred normal path is:

```text
FeatureHistory
→ ScalarWindow
```

### Boundary

Do not remove RawWindow support in this pass.

Do not force all frequency evaluation into FeatureHistory if current frequency path depends on RawWindow.

---

## 9. Reuse inspection mechanic for AMP-first and frequency-first

### Target

Both directions should use the same mechanism:

#### AMP-first

```text
Amp SignalCandidate
→ SignalInspector
→ inspect frequency evidence
→ InspectedSignal
```

#### Frequency-first

```text
FrequencyMatch SignalCandidate
→ SignalInspector
→ inspect AMP locality
→ InspectedSignal
```

This should not require two separate inspectors.

Preferred:

```text
SignalInspector
→ list of InspectionRules
```

Rules decide whether they apply by source/kind.

---

## 10. Prepare broadband / tonal-rejection inspection later

### Target

Do not implement white-noise detection yet.

But design inspection so later this is possible:

```text
Broadband SignalCandidate
→ SignalInspector
→ AddTonalRejection
→ AddBroadbandStats
→ InspectedSignal
```

Do not add full broadband chain now.

Only avoid hardcoding inspection to AMP/frequency.

---

## 11. Logging requirements

Update logs so inspection facts are visible.

Useful `INSPECTED` log fields:

```text
source
kind
accepted
rejectReason
durationMs
strength
confidence
ampPeak
ampMean
ampSupport
locality
duplicateRisk
freqConfidence
```

For window-based AMP locality, log enough to see whether locality came from:

```text
snapshot
ScalarWindow
RawWindow
```

Prefer candidate-level logs only.

Avoid noisy per-loop logs.

---

## 12. Success criteria

After this pass:

```text
SignalInspector uses reusable InspectionRules or is clearly structured toward them.

Frequency-first AMP locality uses retrospective AMP ScalarWindow, not only snapshot.

ScalarWindow from FeatureHistory exists or has a minimal adapter.

WindowEvaluator logic exists for basic scalar stats / AMP support.

RawWindow remains available as fallback / advanced path.

AMP-first and frequency-first use the same SignalInspector mechanism.

InspectedSignal contains AMP/locality facts from window evaluation.

PatternAssembler still creates PatternCandidates.

PatternRules still create PatternResults.

FrequencyMatchDetector still does not own AMP locality.
```

---

## 13. Do not do in this pass

Do not:

```text
remove legacy AMP
introduce DetectionProfile
introduce external strategy config
rewrite FrequencyMatchDetector
implement chirp grouping
implement white-noise chain
implement object detection
move behavior logic
perform heavy threshold tuning
```

This pass is only about the shared inspection mechanic.

---

