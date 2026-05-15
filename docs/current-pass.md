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

Section B focuses only on the signal layer:

SignalCandidate
→ SignalInspector
→ InspectedSignal
Roadmap Section B items
Stabilize SignalCandidate structure
Stabilize InspectedSignal structure
Add signal acceptance / rejection reasons
Add duration / strength / confidence fields
Add duplicate-risk annotation
Add source tags and detector provenance consistently
Support multiple SignalDetector implementations under one signal layer
1. Stabilize SignalCandidate
Target

SignalCandidate is the common low-level candidate emitted by all signal emitters.

It should represent:

Something signal-like may have happened around t.

It must not represent:

valid chirp
near tonal pulse
far tonal pulse
behavior trigger
Required fields

Ensure SignalCandidate consistently supports:

SignalSource source;
SignalKind kind;

uint32_t startMs;
uint32_t peakMs;
uint32_t endMs;

float strength;
float confidence;

SignalDetectorKind detectorKind;

Use existing field names if already present. Do not rename everything unless needed.

Source / kind examples

Preferred:

SignalSource::Amp
SignalSource::TargetFrequency
SignalSource::Broadband // later

SignalKind::AmpTransient
SignalKind::FrequencyMatch
SignalKind::BroadbandTransient // later

Avoid:

FrequencyTransient

where the active frequency detector is now FrequencyMatchDetector.

2. Stabilize InspectedSignal
Target

InspectedSignal is the result of signal-level inspection.

It may be:

accepted
rejected
weak
duplicate-risk
annotated

It is still not a pattern.

It must not represent:

valid chirp
noise burst
nearTonalPulse
PatternResult
Required fields

Add or normalize fields like:

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

If some fields are premature, add enums / placeholders and default them safely.

3. Add signal acceptance / rejection reasons
Target

Rejections should be explainable at signal stage.

Create or stabilize:

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

Use only the reasons currently needed. Do not overfit all future cases.

Rule

SignalInspector decides signal acceptance/rejection.

PatternRules should not rediscover basic signal acceptance.

4. Add duration / strength / confidence fields
Target

Every inspected signal should expose basic comparable facts:

duration
strength
confidence

These should be available regardless of whether the source is AMP or frequency.

Examples:

durationMs = endMs - startMs;
strength = candidate.strength;
confidence = computed or defaulted;

Do not invent complex scoring yet.

Use conservative defaults:

confidence = 1.0f for accepted known-good candidates
confidence = 0.0f for rejected candidates

or preserve existing confidence if already available.

5. Add duplicate-risk annotation
Target

Duplicate risk belongs to signal inspection, not pattern meaning.

Add fields:

bool duplicateRisk;
float duplicateRiskScore;

or equivalent.

Initial implementation can be simple:

mark duplicateRisk if candidate occurs too soon after previous accepted signal from same source

Do not block all duplicates unless existing behavior already does so.

This pass may only annotate duplicate risk; later PatternRules or Behavior can decide how to use it.

6. Add source tags and detector provenance consistently
Target

Every SignalCandidate / InspectedSignal should preserve:

which source produced it
which detector produced it
which signal kind it represents

Example:

source = SignalSource::TargetFrequency
kind = SignalKind::FrequencyMatch
detectorKind = SignalDetectorKind::FrequencyMatch

For AMP:

source = SignalSource::Amp
kind = SignalKind::AmpTransient
detectorKind = SignalDetectorKind::Transient

This is important for logs, Analyzer, and later DetectionProfile work.

7. Support multiple SignalDetector implementations
Target

The signal layer must not assume that every signal comes from TransientDetector.

Current implementations:

TransientDetector
FrequencyMatchDetector

Future possible implementations:

DipDetector
PlateauDetector
ThresholdCrossingDetector

Add or stabilize:

enum class SignalDetectorKind {
  Transient,
  FrequencyMatch,
  Dip,
  Plateau,
  ThresholdCrossing,
  Unknown
};

Do not implement future detectors now.

Only make the type system ready.

8. Logging requirements

Update logs enough to inspect signal-stage correctness.

Preferred stage labels:

SIGNAL
INSPECTED

A useful signal log should show:

source
kind
detectorKind
startMs
peakMs
endMs
durationMs
strength
confidence

A useful inspected log should show:

accepted
rejectReason
ampSupport
locality
duplicateRisk
confidence

Do not create noisy per-loop logs. Log candidate-level events only.

9. Boundaries to preserve

Do not let SignalInspector emit PatternCandidate.

Correct:

SignalCandidate
→ SignalInspector
→ InspectedSignal
→ PatternAssembler

Do not let SignalInspector emit PatternResult.

Correct:

PatternRules
→ PatternResult

Do not let Behavior consume SignalCandidate or InspectedSignal.

Correct:

Behavior consumes PatternResult + FieldState
10. Success criteria

After this pass:

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
11. Do not do in this pass

Do not:

introduce DetectionProfile
introduce external config
rewrite FrequencyMatchDetector
implement chirp grouping
implement white-noise detection
implement new detectors
move behavior logic
change output sound behavior unless required for compile fixes

This pass is signal-layer stabilization only.