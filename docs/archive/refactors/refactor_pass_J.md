# Codex Pass — Section J: DetectionProfile Composition

Version: Detection Roadmap v0.3 — Pass J  
Scope: Introduce `DetectionProfile` as the highest-level code-defined composition item

---

## Goal

Introduce `DetectionProfile` as the highest-level composition layer for the detection stack.

A profile should define how a Resonant mode wires together:

```text
FeatureExtractors
FeatureStreams / FeatureHistory
SignalEmitters
SignalDetectors
SignalInspector / InspectionRules
PatternAssembler
PatternRules
FieldStateConfig
Behavior-facing output

This pass should prove profile composition with the focused profile set:

FreqAmpProfile
AmpStateProfile
ChirpProfile

Do not introduce external JSON/YAML configuration.

Do not build a plugin registry.

Do not implement white-noise, woodblock, or object-like detection.

Roadmap Section J items
Introduce code-defined detection profile factories
Define FreqAmpProfile
Define AmpStateProfile
Define ChirpProfile
Let profiles select feature extractors
Let profiles select signal emitters and signal detectors
Let profiles select inspection rules
Let profiles select pattern assembler
Let profiles select pattern rules
Let profiles select field-state config
Support profile selection at compile-time or simple runtime mode
Avoid external profile configuration
Park WhiteNoiseRoomProfile and WoodBlockProfile
Use DetectionProfile as highest-level composition item
1. Introduce DetectionProfile
Target

DetectionProfile is the top-level composition object for a Resonant detection mode.

It should answer:

Which detection stack is active?

It should not answer:

What pattern does this candidate mean?
How should behavior react?
How does FrequencyMatchDetector work internally?

Those remain owned by:

PatternRules
Behavior
SignalDetector
Suggested shape

Use project style, but aim for something like:

struct DetectionProfile {
  DetectionProfileKind kind;
  const char* name;

  // selected components or factories
  FeatureExtractorSet featureExtractors;
  SignalEmitterSet signalEmitters;
  SignalInspectorConfig inspectorConfig;
  PatternAssemblerConfig patternAssemblerConfig;
  PatternRulesConfig patternRulesConfig;
  FieldStateConfig fieldStateConfig;
};

If this is too heavy for the current code, use a lighter profile factory function first.

2. Use code-defined profile factories
Target

Profiles are defined in code.

Preferred:

DetectionProfile makeFreqAmpProfile();
DetectionProfile makeAmpStateProfile();
DetectionProfile makeChirpProfile();

or direct setup functions:

void configureFreqAmpProfile(DetectionRuntime& runtime);
void configureAmpStateProfile(DetectionRuntime& runtime);
void configureChirpProfile(DetectionRuntime& runtime);

Choose whichever fits current code best.

Avoid

Do not introduce:

JSON config
YAML config
runtime plugin registry
dynamic rule loading
reflection-based fact maps
external profile files

Profiles should be code-defined for now.

3. Define DetectionProfileKind
Target

Add a small enum or equivalent:

enum class DetectionProfileKind {
  FreqAmp,
  AmpState,
  Chirp
};

Optional parked values may exist only as comments, not implemented cases:

// WhiteNoiseRoom later
// WoodBlock later

Do not implement parked future profiles.

4. Define FreqAmpProfile
Purpose

Main current baseline profile.

It proves:

FrequencyMatch detection
+ AMP locality inspection
+ locality-aware tonal pulse pattern meaning
Target composition
FeatureExtractors:
- AMP / ampEnv support
- frequency evidence support as currently needed

SignalEmitters:
- FrequencySignalEmitter

SignalDetectors:
- FrequencyMatchDetector

SignalInspector / InspectionRules:
- basic timing / strength
- frequency facts if needed
- AMP stats
- AMP locality
- duplicate-risk if already available

PatternAssembler:
- single-signal pulse assembler

PatternRules:
- tonal pulse with locality

FieldStateConfig:
- basic activity / quiet / busy
Expected output

Examples:

TonalPulseNear
TonalPulseMid
TonalPulseFar
TonalPulseUnknownLocality
Residual / Rejected

or equivalent current result vocabulary.

Boundary

FrequencyMatchDetector must not own:

AMP locality
PatternAssembly
PatternRules
Behavior decisions
5. Define AmpStateProfile
Purpose

Proves:

AMP signal detection
+ FieldState-driven behavior boundary

This profile is not just an AMP reference. It is meant to prove that behavior can vary based on FieldState without reading signal internals.

Target composition
FeatureExtractors:
- AMP / ampEnv
- ambient/activity support if already available

SignalEmitters:
- AmpSignalEmitter

SignalDetectors:
- TransientDetector

SignalInspector / InspectionRules:
- basic timing
- AMP stats
- duplicate-risk if already available

PatternAssembler:
- single-signal pulse assembler

PatternRules:
- simple AMP pulse / activity pulse

FieldStateConfig:
- quiet / busy / density / activity windows
Behavior boundary

Behavior may use:

PatternResult + FieldState

Behavior must not use:

SignalCandidate
InspectedSignal
FeatureStream
ampEnv directly
6. Define ChirpProfile
Purpose

First real pattern profile.

It proves:

multiple InspectedSignals
→ PatternAssembler
→ PatternCandidate
→ PatternRules
→ PatternResult
Target composition

Initially reuse the same signal layer as FreqAmpProfile if that is the safest path.

SignalEmitters:
- FrequencySignalEmitter, or current useful signal emitter set

SignalDetectors:
- FrequencyMatchDetector

SignalInspector / InspectionRules:
- frequency facts
- AMP locality if useful
- basic timing
- duplicate-risk

PatternAssembler:
- chirp-capable assembler, or scaffold that can group recent pulse-like InspectedSignals

PatternRules:
- onePulse
- pulseSequence
- validChirp later
- invalidChirp / wrongTiming / tooDense later

FieldStateConfig:
- activity / chatter / quiet-busy if already available
Limit

Do not make ChirpProfile a full artistic behavior implementation in this pass.

Allowed now:

select ChirpProfile
use a chirp-capable PatternAssembler path
produce simple PulseSequence PatternCandidates if safe

Not required now:

full chirp behavior
family matching
advanced timing rules
complex acoustic communication
7. Let profiles select feature extractors
Target

Profiles should define which feature extractors / feature streams they need.

Examples:

FreqAmpProfile:
- frequency evidence support
- AMP support

AmpStateProfile:
- AMP support
- ambient/activity support

ChirpProfile:
- same as FreqAmp initially, plus enough signal history for pulse grouping

Do not add unused feature extractors for future profiles.

8. Let profiles select signal emitters and signal detectors
Target

Profiles should select their signal sources.

Examples:

FreqAmpProfile:
FrequencySignalEmitter + FrequencyMatchDetector

AmpStateProfile:
AmpSignalEmitter + TransientDetector

ChirpProfile:
FrequencySignalEmitter + FrequencyMatchDetector

This proves that:

SignalEmitter role stays the same;
SignalDetector implementation may change.
9. Let profiles select inspection rules
Target

Profiles should choose inspection rules appropriate to their signal path.

Examples:

FreqAmpProfile:
AddAmpStats
AddAmpLocality
AddFrequencyFacts
AddDuplicateRisk

AmpStateProfile:
AddAmpStats
AddBasicTimingFacts
AddDuplicateRisk

ChirpProfile:
AddAmpLocality
AddFrequencyFacts
AddDuplicateRisk

Do not introduce a dynamic rule registry.

Simple code-defined lists or setup functions are enough.

10. Let profiles select pattern assembler
Target

Profiles should choose the pattern assembly strategy.

Examples:

FreqAmpProfile:
single-signal pulse assembler

AmpStateProfile:
single-signal pulse assembler

ChirpProfile:
chirp-capable / pulse-sequence assembler

This is the key proof that pattern assembly is profile-selectable.

11. Let profiles select pattern rules
Target

Profiles should choose pattern interpretation.

Examples:

FreqAmpProfile:
locality-aware tonal pulse rules

AmpStateProfile:
simple AMP pulse / activity rules

ChirpProfile:
pulse / sequence / chirp rules

Pattern meaning remains in PatternRules, not in profile selection code.

The profile only chooses which rules are active.

12. Let profiles select field-state config
Target

Profiles should choose the active field-state windows and thresholds.

Examples:

FreqAmpProfile:
basic activity / recent pattern count

AmpStateProfile:
quiet / busy / activity / density are important

ChirpProfile:
activity / chatter / recent pulse density may be useful

FieldStateTracker remains shared infrastructure.

FieldStateConfig is profile-selected.

13. Support simple profile selection
Target

Provide a simple profile selection mechanism.

Acceptable:

compile-time constant
enum selected in code
simple serial command if already easy
simple runtime mode variable

Avoid:

external config files
dynamic profile loading
UI/profile manager
plugin registry
Example
DetectionProfileKind activeProfile = DetectionProfileKind::FreqAmp;

or:

runtime.setProfile(makeFreqAmpProfile());
14. Park future profiles
Target

Do not implement these now:

WhiteNoiseRoomProfile
WoodBlockProfile
ObjectHitProfile
ChimeProfile

They may remain documented as future possibilities.

This pass should prove the profile mechanism, not broaden detection scope.

15. Logging requirements

Add profile-level logs at startup or profile switch.

Useful fields:

PROFILE
activeProfile=FreqAmp/AmpState/Chirp
emitters=...
detectors=...
inspectorRules=...
patternAssembler=...
patternRules=...
fieldStateConfig=...

Do not log profile composition every loop.

Behavior logs should still describe decisions through:

PatternResult + FieldState

not through profile internals.

16. Success criteria

After this pass:

DetectionProfile exists as a code-level concept or clear factory structure.

FreqAmpProfile can be selected.

AmpStateProfile can be selected.

ChirpProfile can be selected or scaffolded.

Profiles choose signal emitters / detectors.

Profiles choose inspection rules.

Profiles choose pattern assembler.

Profiles choose pattern rules.

Profiles choose FieldStateConfig.

No external runtime config is introduced.

WhiteNoiseRoom / WoodBlock / object chains remain parked.

Behavior still consumes PatternResult + FieldState only.

FrequencyMatchDetector still owns only frequency candidate lifecycle.

Current status:

```text
DetectionProfile is now a code-defined composition object.

FreqAmp, AmpState, and Chirp are selectable and logged with their component choices.

Profiles carry feature-set, emitter, detector, inspection, pattern-assembler, pattern-rules, and field-state selections in code.

No external profile config or plugin registry was added.
```
17. Do not do in this pass

Do not:

introduce JSON/YAML config
build a plugin system
implement white-noise detection
implement woodblock/object detection
remove legacy AMP unless already gated and safe
rewrite FrequencyMatchDetector
move AMP locality into FrequencyMatchDetector
move behavior logic into detection profiles
make profiles own pattern meaning directly
perform heavy threshold tuning

This pass is DetectionProfile composition only.

## 18. Test details

Quick smoke checks for this pass:

```text
Build:
- platformio run -e esp32dev
- platformio run -e esp32dev-analyzer

Startup:
- confirm RB prints activeProfile plus component choices at boot

Profile switch:
- RB PROFILE name=freqamp
- RB PROFILE name=ampstate
- RB PROFILE name=chirp
- confirm each switch prints the new profile and its component metadata

Sanity:
- run one short frequency-first SEQ pass
- confirm behavior logs still use PatternResult + FieldState
```
