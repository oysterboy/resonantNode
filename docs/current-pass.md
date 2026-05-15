# Codex Pass 5.3 — Restore Frequency Evidence Detector, Stop Forcing Frequency Through Scalar

## Context

We discovered an architecture drift in the detection refactor.

The old frequency path was not a scalar transient detector.

Old behavior:

```txt
FrequencyCandidateBuilder
= frequency lifecycle state machine
= consumes frequency evidence windows
= opens/stays open/closes based on matched frequency evidence
= tracks best evidence by contrast/score
= closes from lastMatched + releaseDebounce

Current drift:

targetFreqEvidence.score
→ ScalarTransientDetector
→ scalar timing candidate

This loses important frequency semantics:

contrast is no longer lifecycle input
matched-window lifecycle is lost
close timing is driven by scalar release, not last matched frequency evidence
best evidence is not selected the same way
SEQ hit rate degraded

We accept having a second detector type.

Goal

Restore the old frequency behavior/exactness as a proper second detector type, while keeping the new roadmap architecture after candidate proposal.

Target architecture:

AMP path:
ampEnv
→ ScalarTransientDetector
→ SignalCandidate(source=amp)

Frequency path:
targetFreqEvidence
→ FrequencyMatchDetector
→ SignalCandidate(source=frequency_primary)

Shared path:
SignalCandidate
→ SignalInspector
→ InspectedSignal
→ PatternAssembler
→ PatternResult
Core Rule

Do not force frequency detection through ScalarTransientDetector.

Use this corrected architecture rule:

SignalCandidate shape is shared.
SignalDetector input shape may differ by detector type.

So:

ScalarTransientDetector consumes scalar samples.
FrequencyMatchDetector consumes frequency evidence windows.
Important Non-Goal

Do not rewrite the old frequency detector from scratch.

This is a conservative refactor:

FrequencyCandidateBuilder
→ FrequencyMatchDetector

Preserve old behavior as much as possible.

Tasks
1. Create / rename detector

Refactor the old frequency builder role into:

FrequencyMatchDetector

Preferred files:

src/detection/FrequencyMatchDetector.h
src/detection/FrequencyMatchDetector.cpp

Acceptable during migration:

FrequencyCandidateBuilder remains internally,
but is wrapped/adapted by FrequencyMatchDetector.

But final public role should be detector-like, not “builder” language.

2. Define frequency evidence input

The detector should not consume a plain float.

Use or create a struct close to:

struct FrequencyEvidence {
    uint32_t ms;
    uint32_t sample;
    float score;
    float contrast;
    bool matched;
};

If the current code already has an equivalent evidence struct, reuse it.

Do not over-expand this yet.

Optional fields only if already available and useful:

float targetHz;
float targetEnergy;
float neighborEnergy;
3. Preserve old lifecycle semantics

The new FrequencyMatchDetector must preserve old FrequencyCandidateBuilder behavior:

open when matching frequency evidence starts
stay open while matching evidence continues
update candidateLastMatchedMs on each matched evidence window
track firstSeenMs / firstSeenSample
track peakMs / peakSample
track best score and contrast
choose best evidence by contrast first, then score
close after candidateLastMatchedMs + releaseDebounce
duration = releaseMs - firstSeenMs
reject too_short / too_long / refractory / score / contrast as before
emit one candidate when closed/ready

Important:

releaseMs should represent the old matched-window lifecycle close point,
not the later scalar-detector observation point.
4. Output new SignalCandidate shape

The detector may preserve old internals, but its output must adapt to the new roadmap candidate type.

Map old frequency candidate output to SignalCandidate or the current equivalent:

source = frequency_primary
firstMs = firstSeenMs
peakMs = peakMs
releaseMs = releaseMs
durationMs = duration
strength = score or selected strength field
contrast = contrast as evidence payload
rejectReason = old reject reason
emitter/detector = frequency_evidence

If SignalCandidate does not yet support contrast, add a generic evidence/metadata field only if simple.

Do not add PatternCandidate logic here.

5. Update FrequencySignalEmitter

FrequencySignalEmitter should no longer feed evidence.score into ScalarTransientDetector.

Instead:

FrequencySignalEmitter
→ receives / builds FrequencyEvidence windows
→ feeds FrequencyMatchDetector
→ emits SignalCandidate(source=frequency_primary)

Keep FrequencySignalEmitter as the source adapter/composer.

Keep detector lifecycle inside FrequencyMatchDetector.

6. Keep ScalarTransientDetector unchanged for AMP

Do not damage the AMP path.

ScalarTransientDetector remains valid for:

ampEnv
broadbandEnv later
simple scalar envelope transients

Allowed:

AmpSignalEmitter uses ScalarTransientDetector
FrequencySignalEmitter uses FrequencyMatchDetector

This is now intended, not asymmetry.

7. Fix SEQ routing clarity

SEQ must make the primary path obvious.

For frequency candidates, logs should say:

emitter=frequency
detector=frequency_evidence
legacy_freq_builder=0
source=frequency_primary

If old FrequencyCandidateBuilder still exists as compatibility internals, do not log it as the active emitter.

If an explicit legacy comparison remains, log it separately:

legacy_comparison=1

But primary SEQ candidate must not say:

emitter=legacy_builder
legacy_freq_builder=1

unless it truly is still old comparison-only output.

8. SEQ report cleanup

The current logs often say:

source=amp_fallback
reject=freq_score_too_low

Make report semantics clear:

If AMP is the accepted primary: say source=amp / fallback=amp.
If frequency evidence is accepted primary: say source=frequency_primary.
If frequency was only comparison/evidence but not accepted: say that explicitly.
Do not make the accepted path ambiguous.
9. Do not tune parameters

Do not change frozen params:

onset=30.0
release=20.0
cooldown=50
releaseDebounce=10
minMs=90
maxMs=240
minStrength=40.0

If FrequencyMatchDetector has its own old params, preserve old values.

This pass is architecture/parity, not tuning.

Expected Result

After this pass, run SEQ at 70cm.

Expected signs of success:

frequency path no longer goes through ScalarTransientDetector
SEQ logs detector=frequency_evidence for frequency candidates
hit rate should move back toward old frequency-builder baseline
duration distribution should look closer to old matched-window lifecycle
no new duplicates / late hits
candidate source is unambiguous

Old reference target:

before refactor 70cm:
expected_hits ≈ 95/100
misses ≈ 5
avg duration ≈ 102 ms
durations heavily around ~96 ms
duplicates = 0
late_hits = 0

Recent bad reference:

05_04:
expected_hits = 86/100
misses = 14
logs still show emitter=legacy_builder / amp_fallback ambiguity
Acceptance Criteria

Pass 5.3 is acceptable if:

frequency detection is no longer forced through scalar input
FrequencyMatchDetector or equivalent exists
it consumes frequency evidence windows, not just score
old matched-window lifecycle semantics are preserved
ScalarTransientDetector remains generic and scalar-only
AMP and frequency emit the same SignalCandidate shape
SEQ primary path logs are unambiguous
no threshold tuning was performed