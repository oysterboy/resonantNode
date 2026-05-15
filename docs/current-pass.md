# Codex Pass: 5.2 Restore FrequencyCandidateBuilder Timing Semantics in Shared ScalarSignalEmitter

## Context

We are implementing the Detection Roadmap v0.2.

Current intended architecture:

FeatureExtractors  
→ FeatureStreams / FeatureHistory  
→ SignalEmitters use SignalDetectors to propose SignalCandidates  
→ SignalInspector accepts/rejects/annotates InspectedSignals  
→ PatternAssembler creates PatternCandidates  
→ PatternRules emit PatternResults  
→ Behavior consumes PatternResults + FieldState

Important rule:

> Primary source changes; inspection mechanic stays the same.

So AMP and frequency should not have different candidate lifecycle mechanics anymore.

## Problem

After the refactor, the new frequency path uses `ScalarTransientDetector::transientDetected()` and finalizes a candidate using the current frame time as release time.

This changed behavior compared to the old `FrequencyCandidateBuilder`.

Old frequency builder behavior preserved a richer lifecycle:

- first seen
- peak
- release
- duration / hold windows
- score
- contrast
- reject reason
- readiness / candidate closed state

The new Scalar path seems to close only after release debounce has elapsed. This likely shifts release timing later and changes duration distribution.

Observed SEQ difference at ~70cm:

- before refactor: expected hits ~95/100, duration heavily around ~96ms
- after refactor: expected hits ~92/100, more durations around 120–136ms
- no late/duplicate explosion, but candidate timing drift is visible

Goal is parity, not tuning.

Do not change detector thresholds yet.

Current frozen params:

```txt
onset=30.0
release=20.0
cooldown=50
releaseDebounce=10
minMs=90
maxMs=240
minStrength=40.0
Goal

Change the shared scalar signal-emitter path so that it preserves the old FrequencyCandidateBuilder timing semantics as closely as possible.

This should happen in the shared scalar candidate lifecycle mechanic, not in a frequency-only special class.

Target shape:

ScalarTransientDetector
  = generic threshold/transient state machine

ScalarSignalEmitter
  = generic scalar candidate lifecycle owner:
    firstSeenMs
    peakMs
    releaseMs
    durationMs
    holdWindows or equivalent active-window count
    strength
    reject reason
    source name

Amp / Frequency
  = configured sources using the same ScalarSignalEmitter mechanic
Important Architecture Constraint

Do NOT move frequency-specific logic into ScalarTransientDetector.

ScalarTransientDetector must stay generic:

no frequency vocabulary
no contrast-specific logic
no PatternCandidate logic
no SEQ reporting logic
no source-specific behavior

Do NOT create a special FrequencySignalEmitter clone of the old FrequencyCandidateBuilder.

Instead:

Extract the reusable lifecycle semantics from FrequencyCandidateBuilder
into the shared ScalarSignalEmitter mechanic.

Temporary thin wrappers/factories are acceptable:

AmpSignalEmitter.cpp
  → configures ScalarSignalEmitter for ampEnv

FrequencySignalEmitter.cpp
  → configures ScalarSignalEmitter for targetFreqEnv

But wrappers must not contain divergent candidate lifecycle logic.

Tasks
1. Inspect old FrequencyCandidateBuilder behavior

Find the old candidate lifecycle logic, especially:

when first_seen_ms is assigned
how peak_ms and peak score are tracked
how release_ms is assigned
how duration is calculated
how hold windows are counted
how reject reasons are assigned
when a candidate becomes ready/closed
how too-short / refractory / score / contrast rejects are represented

Use this as the parity reference.

2. Inspect current Scalar path

Inspect:

ScalarTransientDetector.cpp
current ScalarSignalEmitter if it exists
AmpSignalEmitter.cpp
FrequencySignalEmitter.cpp
AnalyzerApp.cpp SEQ plumbing

Find where current frequency candidates are finalized from:

ScalarTransientDetector::transientDetected()

and where release time is set from current frame time.

3. Move reusable lifecycle tracking into ScalarSignalEmitter

Update or create ScalarSignalEmitter so it owns candidate lifecycle tracking around ScalarTransientDetector.

It should track, generically:

firstSeenMs
peakMs
releaseMs
durationMs
holdWindows
peakStrength
currentStrength
rejectReason
source

The scalar emitter should be able to emit candidates for different scalar streams:

ampEnv
targetFreqEnv
broadbandEnv // later

Frequency and AMP should use the same lifecycle mechanism.

4. Restore old release/duration semantics

Do not simply set releaseMs = now when transientDetected() becomes true.

Instead, preserve the old builder-style release semantics as closely as possible.

Expected behavior:

firstSeenMs marks the first active/onset moment
peakMs marks the frame/window with highest scalar value
releaseMs reflects the actual release/close point according to old builder semantics, not an extra-late observation point after debounce
durationMs = releaseMs - firstSeenMs
duration distribution should remain comparable to old builder output

If exact old behavior cannot be reproduced without changing ScalarTransientDetector, add the minimal generic detector-facing accessors/events needed, such as:

onsetStarted()
releaseObserved()
candidateClosed()
candidateRejected()

or equivalent.

Keep those generic.

5. Keep ScalarTransientDetector generic

Allowed changes:

expose generic lifecycle state/events if needed
expose last onset/release/peak timing if already conceptually generic
improve naming around active/closed/rejected state

Not allowed:

add frequency-specific fields
add contrast-specific decisions
add PatternCandidate logic
add SEQ-specific behavior
add source names inside detector
6. Make AMP and frequency use the same scalar emitter mechanic

After the change:

AMP path:
ampEnv → ScalarSignalEmitter → ScalarTransientDetector → SignalCandidate(source=amp)

Frequency path:
targetFreqEnv → ScalarSignalEmitter → ScalarTransientDetector → SignalCandidate(source=frequency_primary)

If AmpSignalEmitter and FrequencySignalEmitter remain, they should only configure source name, stream input, params, and optional feature labels.

They should not implement different lifecycle timing.

7. Clean up Analyzer SEQ comparison path

Analyzer SEQ must not silently mix old FrequencyCandidateBuilder output with new roadmap emitter output.

Do one of these:

Preferred:

route SEQ frequency candidate reporting through the new scalar emitter path

Acceptable during migration:

keep old builder only as explicit comparison output
log it clearly as legacy comparison
ensure primary SEQ result says whether candidate came from:
scalar_emitter
legacy_frequency_builder
amp_fallback

Do not let old and new paths be ambiguous.

8. Logging requirements

Update logs so this is obvious:

For each emitted SignalCandidate / SEQ candidate, log:

source=
emitter=
first_ms=
peak_ms=
release_ms=
duration_ms=
strength=
reject=

If legacy builder is still present, log:

legacy_freq_builder=1

or equivalent.

If using new scalar emitter, log:

emitter=scalar
Non-goals

Do not tune thresholds.

Do not change:

onset
release
cooldown
releaseDebounce
minMs
maxMs
minStrength

Do not implement:

PatternAssembler
PatternRules changes
FieldState changes
Behavior changes
frequency-family matching
overlap dominance
acoustic profile system
full DetectionStrategy/Profile cleanup

This is a parity/correction pass only.

Expected Result

After this pass, rerun SEQ at ~70cm with frozen params.

Expected signs of success:

expected_hits near previous baseline, ideally ~95/100
misses not increased
duplicates remain 0 or near 0
late_hits remain 0
duration distribution moves back toward old ~96ms-heavy shape
release/duration no longer shifted longer by the new close observation timing
SEQ logs clearly identify scalar emitter vs legacy builder path
Acceptance Criteria

Pass is acceptable if:

ScalarTransientDetector remains generic
reusable candidate lifecycle semantics live in ScalarSignalEmitter or equivalent shared scalar emitter mechanic
AMP and frequency no longer have divergent lifecycle logic
frequency candidate timing matches old FrequencyCandidateBuilder behavior more closely
Analyzer SEQ output is not mixing old/new paths ambiguously
no parameter tuning was performed