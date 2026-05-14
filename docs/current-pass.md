# ResonantNode Roadmap - Audio / Detection Architecture

Scope: this roadmap is for the local ResonantNode firmware.

It is not a full VEKTOR-system roadmap.

Out of scope here:

```text
hub scheduling
host OSC API
transport bindings
full field protocol
multi-node snapshot logic
complete resource registry
```

This roadmap sequences implementation so the firmware can evolve from the current tonal-transient implementation toward reusable acoustic pattern profiles without turning the current detector into the whole architecture.

When roadmap wording is ambiguous, `myspec.md` is the authority.

---

## Roadmap Principle

Do not roadmap all possible acoustic intelligence at once.

Roadmap the smallest stable path from current tonal transient detection toward reusable acoustic pattern profiles.

Current direction:

```text
stabilize current tonal transient path
-> clean detector boundaries
-> add frequency-first transient detection
-> add simple acoustic field state
-> introduce pattern profiles by composition
-> add pulsed chirp
-> later add other acoustic families
```

---

## Phase 0 - Architecture Language Freeze

Goal:

```text
Stop naming and responsibility drift before more code is added.
```

Authority:

```text
When roadmap wording is ambiguous, defer to myspec.md.
```

Define current terms using the myspec vocabulary:

```text
Current implementation:
    TonalTransient / TonalPulse, not Chirp.

PatternResult:
    behavior-facing event meaning.

AcousticFieldState:
    acoustic context separate from PatternResult.

PatternProfile:
    composition-level bundle for an implementation-specific acoustic detection strategy, with behavior mapping.

AudioSignal:
    currently still contains signal material, raw history, and transitional candidate assembly.
```

Update current-pass documentation with the actual current flow:

```text
AudioSignal
-> Feature Evidence
-> Candidate / PatternCandidate
-> PatternResult
-> ResonantBehavior
```

If the code still uses a lower-level detector-emitted candidate internally, treat that as a transitional implementation detail under Candidate, not as a separate architecture layer.

Avoid reverting to the older mental model:

```text
AudioSignal
-> Detector
-> Behavior
```

Current rule:

```text
Behavior consumes PatternResult, not raw detector flags or live feature states.
```

Deliverable:

```text
docs/current-pass.md or equivalent reflects the current architecture vocabulary.
```

---

## Phase 1 - Stabilize Current TonalTransient A-Path

Goal:

```text
Make the current working path reliable and inspectable.
```

Current path:

```text
AMP / transient candidate
-> RawSampleHistory
-> candidate-window frequency measurement
-> ValidTonalTransient / TransientOnly / Invalid
-> Behavior
```

Tasks:

```text
keep AMP/transient baseline frozen
keep RawSampleHistory
keep FrequencyWindowProbe
improve logs around early/full frequency evidence
confirm behavior with requireTonal = off / on
avoid adding more detector logic into AudioSignal
avoid relying on AudioFrequencyDetector transient flags for behavior
```

This phase validates the low-risk diagnostic path:

```text
AMP candidate defines the event window.
Raw history provides candidate-window frequency evidence.
```

Deliverable:

```text
Known-good TonalTransient prototype.
```

---

## Phase 2 - Detector Cleanup / Reusable Scalar Detector

Goal:

```text
Clarify what "detector" means before adding more detection paths.
```

Current issue:

```text
AudioOnsetDetector:
    amplitude-envelope / scalar stream extractor plus transient detector,
    but still audio / amplitude specific in naming and usage.

AudioFrequencyDetector:
    frequency stream extractor
    + duplicate onset/transient logic.

AudioSignal:
    signal material
    + raw history
    + candidate assembly.
```

Target concepts:

```text
StreamExtractor:
    produces scalar / evidence stream

AmpEnvelopeStreamExtractor:
    derives an amplitude-envelope stream from raw samples

FrequencyBandStreamExtractor:
    derives a frequency-band evidence stream from raw samples

ScalarTransientDetector:
    shared onset / release / transient core for any scalar stream

CandidateBuilder:
    turns detector facts into candidate objects

PatternDetector:
    turns PatternCandidate into PatternResult
```

Suggested steps:

```text
2.1 Rename/comment current detector roles.
2.2 Extract or define ScalarTransientDetector from AudioOnsetDetector.
2.3 Keep AudioSignal candidate assembly for now.
2.4 Later move AmpCandidateBuilder out of AudioSignal.
2.5 Reduce AudioFrequencyDetector to FrequencyBandStreamExtractor,
    or mark its transient logic as prototype only.
2.6 Keep the amplitude side conceptually parallel: raw samples -> AmpEnvelopeStreamExtractor -> ScalarTransientDetector.
```

Avoid:

```text
full AudioSignal rewrite
adding more custom transient logic to AudioFrequencyDetector
routing frequency detector flags directly to behavior
```

Deliverable:

```text
Reusable scalar onset/transient detection concept ready for amplitude-envelope and frequency streams.
```

---

## Phase 3 - Frequency-First Transient Detection

Goal:

```text
Detect tonal beep/click events directly from a frequency-band evidence stream.
```

Note:

```text
Do not rename the public detector / candidate classes yet.
Pass 3 should prove the stream architecture first; renames (also see pass 2) can wait until the shape is stable.
ScalarTransientDetector is the shared core we use to reduce duplicated transient logic in the wrappers, not a new permanent public detector layer.
```

Pass 3 fix scope:

```text
Keep FrequencyBandStreamExtractor as-is.
liveFreq remains the live 64-sample rolling frequency stream.
Keep probe64 as diagnostic-only.

Primary goal:
Make live frequency detection measurable as a future frequency-first path.

Do NOT turn retrospective scanning into the detector.
Do NOT use retrospective scan results for candidate_valid, tonal_valid, behavior_eligible, PatternResult validity, or behavior decisions.

Add a diagnostic-only live-frequency gate:
- track first_seen, peak, release, score, contrast, and hold duration
- use live-only thresholds, separate from retrospective freqEarly/freqFull tuning
- keep AMP behavior unchanged
- keep probe64 diagnostic-only

The live-frequency proof result is:
- liveFreq can independently produce a timestamped event
- retrospective qualification stays diagnostic-only
- the next step is to shape a stable FrequencyCandidate from the live stream
```

Expected output:
SEQ should clearly compare:
AMP candidate timing
liveFreq first/best timing
retrospective best 64-sample probe timing

Phase 3 outcome:

```text
Live frequency eventing is proven.
Next: stabilize the live FrequencyCandidate shape and keep AMP only as the comparison baseline.
```

Decision rule:
If liveFreq has stable threshold crossings near AMP onset/peak, continue toward a causal frequency-first detector.
If retrospective best matches liveFreq but old freqEarly/freqFull fails, old candidate-window summary is too broad/wrong.
If liveFreq and retrospective 64-sample scan disagree, inspect buffer mapping, sample indexing, Goertzel/bin config, and neighbor-band calculation.
```

This is the C path:

```text
AudioSignal
-> FrequencyBandStreamExtractor / TargetBandEnvelope
-> OnsetDetector
-> TransientDetector
-> FreqTransientCandidate
-> PatternCandidate
-> PatternResult
```

Reason:

For tonal beeps/clicks, the target-band stream may be cleaner than broadband amplitude.
This stream is live and separate from `FrequencyWindowProbe`, which remains a retrospective candidate-window helper.

It may suppress:

```text
room reflections
mechanical body noise
tail energy
broadband clicks
off-band artifacts
```

Initial use:

```text
FreqTransientCandidate = diagnostic / analyzer comparison
```

Compare against the current A-path:

```text
AMP candidate exists?
FreqTransient exists?
FreqTransient timing?
FreqTransient score?
Which one catches expected beeps better?
Which one reduces late hits / duplicates?
```

Do not immediately make behavior depend on frequency-first candidates.

Suggested sequence:

```text
3.1 Produce target-band / contrast stream.
3.2 Feed it into reusable ScalarTransientDetector.
3.3 Emit FreqTransientCandidate.
3.4 Log and compare against AmpCandidate.
3.5 Only later allow PatternResult to prefer or require frequency-first detection.
```

Note for pass 2 cleanup:

```text
These are still pass 2 items, but pass 3 is the checkpoint that tells us when they are safe to do.
2.4 Move AmpCandidateBuilder out of AudioSignal only after pass 3 has proven the stream split is stable and candidate assembly no longer changes shape.
2.5 Reduce AudioFrequencyDetector to FrequencyBandStreamExtractor only after the pass 3 comparison path is stable enough that its transient logic is clearly duplicative.
2.6 Keep the amplitude side conceptually parallel: raw samples -> AmpEnvelopeStreamExtractor -> ScalarTransientDetector.
```

Deliverable:

```text
Frequency-first transient detection compared against AMP-first A-path.
```
---

## Phase 3B - 

Use the comparison from phase 3  only until you can answer:
Can liveFreq independently produce a stable timestamped event?

Then retire most of the comparison scaffolding.

Next target should be:

FrequencyCandidate {
  first_cross_ms
  peak_ms
  peak_score
  peak_contrast
  duration_or_hold_ms
  release_ms
}

Then compare that to AMP only for validation.
---

## Phase 4 - AcousticFieldState v0

Goal:

```text
Add acoustic context without confusing it with PatternResult.
```

Difference:

```text
PatternResult:
    What event happened?

AcousticFieldState:
    What is the surrounding acoustic field like?
```

First version should be simple:

```text
ambientLevel
activityAverage
candidateCountLastWindow
quietMs
isQuiet
isBusy
```

Behavior may later use it for:

```text
suppress response when field is busy
allow idle emit only after quiet
adapt wait / refractory
reduce response probability during dense activity
```

Do not create fake pattern results like:

```text
BUSY_ROOM
CHATTER
QUIET
```

These belong to `AcousticFieldState`, not `PatternResult`.

Deliverable:

```text
Compact acoustic context summary available for logs and later behavior input.
```

---

## Phase 5 - PatternProfile Structure

Goal:

```text
Prevent the current TonalTransient implementation from becoming the whole architecture.
```

Introduce a lightweight profile concept by composition, not inheritance.

Shared firmware scaffold:

```text
SoundInput
AudioSignal
RawSampleHistory
SoundOutput
Timing
Parameters
Commands
State / Events
DetectionPipeline contracts
Analyzer support
Behavior boundary
```

Profile-specific:

```text
evidence extractors
candidate builders
pattern detectors
thresholds
classifier rules
behavior mapping
analyzer scoring
```

Avoid:

```cpp
class TonalTransientNode : public ResonantNode
class PulsedChirpNode : public ResonantNode
class GlassChimeNode : public ResonantNode
```

Prefer:

```text
ResonantNodeApp
+ PatternProfile
+ shared objects
```

First real profile:

```text
TonalTransientProfile
```

Deliverable:

```text
Current tonal transient logic is named and bounded as one implementation profile.
```

---

## Phase 6 - PulsedChirpProfile

Goal:

```text
Add first real multi-event acoustic pattern family.
```

Detection model:

```text
single tonal pulse = event candidate
pulsed chirp = temporal pattern over multiple tonal pulse candidates
```

Pipeline:

```text
TonalPulseCandidate(s)
-> ChirpGroupBuilder
-> PulsedChirpPatternDetector
-> PatternResult
```

Pulsed chirp evaluation may use:

```text
pulse count
inter-pulse gaps
gap consistency
total span
per-pulse duration
per-pulse confidence
frequency-group consistency
optional frequency movement across pulses
```

Do not treat pulsed chirp as one transient.

Do not call current tonal transient a chirp.

Deliverable:

```text
First grouped-event PatternProfile.
```

---

## Phase 7 - Later Acoustic Families

Keep later / volatile:

```text
continuous tonal chirp trajectory detection
glass chime spectral / decay profile
woodblock / knock profile
white-noise room profile
parallel candidate correlation
dense-field ambiguity
family matching
VEKTOR pattern configuration
```

These are valid future profiles or capabilities, but they should not block stabilizing the current firmware.

---

## Roadmap Table

| Phase | Name | Main goal | Avoid |
|---|---|---|---|
| 0 | Architecture language freeze | stop naming drift | new abstractions before docs catch up |
| 1 | TonalTransient A-path stabilization | reliable current path | behavior changes from unvalidated evidence |
| 2 | Detector cleanup | reusable scalar detector concept | full rewrite of AudioSignal |
| 3 | Frequency-first transient detection | detect beeps on target-band stream | routing freq flags directly to behavior |
| 4 | AcousticFieldState v0 | context input: quiet/busy/activity | fake PatternResults like BUSY_ROOM |
| 5 | PatternProfile structure | composition-based implementation profiles | subclass-per-pattern |
| 6 | PulsedChirpProfile | first grouped-event pattern | calling single tonal transients "chirps" |
| 7 | Later profiles | glass/chime/noise/etc. | overgeneralizing now |

---

## Current Decision Summary

```text
A now:
    AMP/transient candidate + raw-history frequency measurement.

C next:
    reusable transient detection over FrequencyBandStream.

B later/volatile:
    parallel AmpCandidate + FreqCandidate correlation.
```

Tradeoff summary:

```text
A = most RAM, least architecture disruption
B = most bookkeeping, highest premature-abstraction risk
C = best long-term stream architecture, more refactor cost
```

Recommended sequence:

```text
A now
C next
B later if needed
```

One-line roadmap:

```text
Stabilize the current TonalTransient implementation, clean detector boundaries just enough, add frequency-first transient detection, add simple acoustic field context, then introduce PatternProfiles by composition before adding pulsed chirp or other acoustic families.
```
