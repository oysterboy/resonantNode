# Codex Pass — Section E: Pattern Layer

## Goal

Stabilize the pattern layer so signal inspection and pattern interpretation are clearly separated.

This pass should formalize:

```text
InspectedSignal
→ PatternAssembler
→ PatternCandidate
→ PatternRules
→ PatternResult

The immediate implementation may remain simple:

one accepted InspectedSignal
→ one pulse-like PatternCandidate

But the architecture must allow later:

multiple InspectedSignals
→ chirp / burst PatternCandidate

Do not introduce DetectionProfile yet.

Do not remove legacy AMP yet.

Do not rewrite FrequencyMatchDetector.

Roadmap Section E items
Stabilize PatternCandidate as its own structure
Stabilize PatternResult as meaning-bearing output
Keep PatternRules as the only pattern interpretation layer
Add single-signal pulse pattern assembly
Add multi-signal chirp / burst pattern assembly
Allow one InspectedSignal to belong to multiple PatternCandidates
Add pulse-count / timing validation
Add residual / invalid / too-dense pattern handling
1. Stabilize PatternCandidate as its own structure
Target

PatternCandidate should be a pattern-layer object, not just a legacy alias.

It represents:

one possible pattern assembled from one or more InspectedSignals

It does not represent:

raw signal event
signal acceptance
final meaning
behavior decision
Suggested fields

Create or stabilize a structure like:

struct PatternCandidate {
  PatternCandidateKind kind;

  uint32_t startMs;
  uint32_t endMs;

  // references or copies of involved inspected signals
  // keep simple if references are awkward on embedded target
  InspectedSignal primarySignal;
  // later: small fixed-size array / vector of signal refs

  uint8_t signalCount;

  float strength;
  float confidence;

  LocalityClass locality;
};

Do not overbuild dynamic containers if the embedded code prefers fixed-size arrays.

If code still uses old `DetectionPipeline::PatternCandidate` / `DetectionPipeline::PatternResult` aliases, replace them only where the include/adaptor change is mechanical. The hard split has already landed, so the remaining work is compatibility cleanup, not a new behavior decision.

2. Stabilize PatternResult as meaning-bearing output
Target

PatternResult is the first object that carries detection meaning for Analyzer / Behavior.

Examples:

tonalPulse
nearTonalPulse
farTonalPulse
validChirp
invalidChirp
noiseBurst
residual
rejected
Suggested fields
struct PatternResult {
  PatternResultKind kind;

  bool valid;

  uint32_t startMs;
  uint32_t endMs;

  float strength;
  float confidence;

  LocalityClass locality;

  PatternRejectReason rejectReason;
};

Use existing names where present.

Boundary

Behavior may consume:

PatternResult + FieldState

Behavior must not consume:

SignalCandidate
InspectedSignal
PatternCandidate
3. Keep PatternRules as the only pattern interpretation layer
Target

Only PatternRules should interpret PatternCandidate into PatternResult.

Correct:

PatternCandidate
→ PatternRules
→ PatternResult

Wrong:

SignalInspector
→ PatternResult

Wrong:

SignalEmitter / SignalDetector
→ PatternResult

Wrong:

PatternAssembler
→ final validChirp meaning
Cleanup target

If PatternAssembler or SignalInspector currently decides pattern meaning, move that decision into PatternRules.

If PatternRules still performs low-level signal acceptance, keep that cleanup minimal and defer if risky, but mark it clearly as compatibility logic.

4. Add single-signal pulse pattern assembly
Target

Formalize the current trivial assembler behavior.

For each accepted InspectedSignal, create one pulse-like PatternCandidate.

Example:

accepted InspectedSignal(source = FrequencyMatch)
→ PatternCandidate(kind = SinglePulse)

or:

accepted InspectedSignal(source = AmpTransient)
→ PatternCandidate(kind = SinglePulse)
Suggested candidate kinds
enum class PatternCandidateKind {
  SinglePulse,
  PulseSequence,
  NoiseBurst,
  ObjectHit,
  Unknown
};

For now, only SinglePulse may be implemented.

Success check

PatternAssembler is the only stage creating PatternCandidate.

5. Prepare multi-signal chirp / burst pattern assembly
Target

Do not fully implement complex chirp detection unless simple and safe.

But prepare the interface so later this is possible:

InspectedSignal A
InspectedSignal B
InspectedSignal C
→ PatternCandidate(kind = PulseSequence, signalCount = 3)
Minimal preparation

PatternAssembler should be shaped around a small recent-signal buffer or a list of current inspected signals.

Possible API:

PatternAssemblyResult PatternAssembler::assemble(
  const InspectedSignal& signal,
  const PatternAssemblyContext& context
);

or:

size_t PatternAssembler::assemble(
  const InspectedSignal* signals,
  size_t signalCount,
  PatternCandidate* out,
  size_t maxOut
);

Use the project’s style.

Do not introduce heap allocation if the code avoids it.

6. Allow one InspectedSignal to belong to multiple PatternCandidates
Target

The architecture should not assume one signal maps to exactly one pattern forever.

Example later:

Signal A
→ PatternCandidate: onePulse
→ PatternCandidate: startOfThreePulse

Example later:

Signals A+B+C
→ PatternCandidate: threePulseChirp
Signals B+C
→ PatternCandidate: partialChirp
Current implementation

For now, one accepted signal may still produce one pattern candidate.

But avoid naming/API choices that make one-to-one mapping permanent.

Prefer:

assemble() may emit zero, one, or many PatternCandidates
7. Add pulse-count / timing validation hooks
Target

Pattern timing belongs to the pattern layer.

Do not implement full chirp timing unless safe.

But introduce names/placeholders for:

pulse count
inter-pulse gap
sequence duration
tooDense
wrongTiming
partialSequence

These should be used later by chirp/burst PatternRules.

Suggested future fields on PatternCandidate
uint8_t pulseCount;
uint32_t firstPulseMs;
uint32_t lastPulseMs;
uint32_t minGapMs;
uint32_t maxGapMs;

Only add if useful now. Otherwise document the intended extension.

8. Add residual / invalid / too-dense pattern handling
Target

Not every assembled candidate should become a valid pattern.

PatternRules should be able to output:

Residual
Invalid
TooDense
Rejected

or equivalent.

Suggested enum
enum class PatternResultKind {
  TonalPulse,
  TonalPulseNear,
  TonalPulseMid,
  TonalPulseFar,
  NoiseBurst,
  ValidChirp,
  InvalidChirp,
  TooDense,
  Residual,
  Rejected,
  Unknown
};

Keep the enum minimal if the existing code already has a smaller vocabulary.

Boundary

A rejected signal belongs to SignalInspector.

A rejected pattern belongs to PatternRules.

Do not conflate:

Signal rejected because weak/invalid

with:

Pattern rejected because timing/count/density is wrong
9. Logging requirements

Add or improve logs for pattern-layer traceability.

Useful stage labels:

PATTERN_CANDIDATE
PATTERN_RESULT

For PatternCandidate, log:

candidateKind
signalCount
source/kind of primary signal
startMs
endMs
strength
locality

For PatternResult, log:

resultKind
valid
confidence
locality
rejectReason

Avoid noisy per-loop logs.

A single event should be traceable as:

SIGNAL
→ INSPECTED
→ PATTERN_CANDIDATE
→ PATTERN_RESULT
10. Success criteria

After this pass:

PatternCandidate is explicit or clearly prepared to become explicit.

PatternResult is clearly the meaning-bearing output.

PatternAssembler creates PatternCandidates.

PatternRules create PatternResults.

SignalInspector does not create PatternCandidates or PatternResults.

Single accepted InspectedSignal can become a pulse-like PatternCandidate.

The PatternAssembler API does not prevent future many-signal pattern assembly.

PatternRules can represent residual / rejected / invalid pattern outcomes.

Behavior still consumes PatternResult + FieldState only.
11. Do not do in this pass

Do not:

remove legacy AMP
introduce DetectionProfile
introduce external config
rewrite FrequencyMatchDetector
move AMP locality into FrequencyMatchDetector
implement full chirp behavior unless trivial
implement white-noise chain
implement object detection
move behavior logic into pattern layer
perform heavy threshold tuning

This pass is pattern-layer stabilization only.
