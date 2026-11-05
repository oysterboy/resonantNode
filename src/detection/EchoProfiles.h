#pragma once

#include <stdint.h>

#include "detectors/scalar/ScalarTransientConfig.h"
#include "inspection/InspectorTypes.h"

/*
EchoProfiles (step 23.3, review 2026-11-05 M5)

The echo modes' detection chains as data: the echo room's (a clap: ScalarTransientDetector on the envelope, the
AttackRise module) and Whispernet's (a phrase: the detector with syllable gaps bridged, the LevelWindow, Modulation and
Flatness modules), each a values struct with the shipped defaults and one builder that turns the values into the
detector config and the inspection plan. The shell (EchoApp, Whispernet) embeds the values and registers them as
echo.* / whisper.* params; native-echoreplay and the host suites start from the same struct, so the PC judges a WAV
with the chain the board runs, not a hand-copied one. DetectionProfile.h is the Node's counterpart.

Owns: the two chains' values, defaults and composition. Does not: hold the board's state (the room level, the open
phrase), decide (Behavior), or size the FeatureHistory (platformio.ini sets that per build; the host envs mirror it).
*/
namespace detection {

// The echo room (ECHO-001). Strength16 levels (the desk 2026-10-31: the quiet room reads 40-100, a click from the
// Emitter at ~1 m ~2,000).
struct ClapChainValues {
    float floor = 300.0f;    // the noise: the attack's low point never below it
    float onset = 1000.0f;   // the detector's onset
    float release = 700.0f;  // the detector's release
    // The occurrence's peak at least this (owner 2026-10-31: permissive, "reacting to the unplanned is the point"):
    // above most of the music's sharp onsets on the desk (1,000-1,230), below a click at ~1 m (1,750-1,940).
    float minPeak = 1200.0f;
    // A clap rises (10 % -> 90 % of its peak) within this. 20 since the five-board desk (2026-11-01): ~60 % of what
    // the boards heard failed at 10, among it the neighbours' replays, which rise more slowly through the speaker.
    uint32_t maxAttackMs = 20;
};

inline void makeClapChain(const ClapChainValues& v, ScalarTransientConfig& d, InspectionPlan& plan) {
    const ScalarTransientConfig fresh;  // every field at its default, then the chain's values
    d = fresh;
    d.observedStream = FeatureStreamId::AmpEnvelope;
    d.onsetDetectionThreshold = v.onset;
    d.onsetReleaseThreshold = v.release;
    d.cooldownAfterOnsetMs = 50;
    d.minTransientDurationMs = 0;
    d.maxTransientDurationMs = 300;
    d.minTransientPeakStrength = v.minPeak;
    d.releaseDebounceMs = 20;
    // One module, the attack; Medium (within maxAttackMs) is a clap.
    plan = InspectionPlan();  // not a brace list: g++ 13 at -O2 fails inside (an internal error) in this inline body
    InspectionModuleConfig& m = plan.modules[0];
    m.kind = InspectionModuleKind::AttackRise;
    m.target = InspectionTarget::Attack;
    m.enabled = true;
    m.minimumStrength = StrengthClass::Medium;
    m.attack.stream = FeatureStreamId::AmpEnvelope;
    m.attack.floorLevel = v.floor;
    m.attack.mediumMaxMs = v.maxAttackMs;
    m.attack.strongMaxMs = v.maxAttackMs / 2;
    m.attack.weakMaxMs = v.maxAttackMs * 3;
    plan.count = 1;
}

// Whispernet (ECHO-002). The owner's whispers: 300-860 from 0.5-1 m, ~150-200 from 2 m; in the afternoon the room
// sat at 150-260, so the thresholds went up (2.5x the room): 2 m whispers are lost. Not the replay tool's scale (a
// WAV maps to the mic's full scale; the synthetic set reads ~16 dB above these). With adaptive on (2026-11-04) onset,
// release and floor are the minimums and their ratios: the thresholds stand at roomFactor x the room's level, never
// under these.
struct WhisperChainValues {
    float onset = 250.0f;     // the detector's onset: a phrase starts
    float release = 175.0f;   // and its release
    float floor = 190.0f;     // the level window's low bound
    float loud = 900.0f;      // the loud bound: above it never a whisper (speech, a clap, the own loop growing)
    float modulation = 0.3f;  // the modulation index a whisper reaches at least (Medium)
    // The spectrum's flatness a whisper lies within (BandFlatness, the median of the phrase's loud blocks): the
    // owner's whispers on the desk read 0.16-0.49 (median ~0.3), speech 0.04-0.41 (median ~0.07), rustling and
    // typing 0.19-0.76 (median ~0.45) (2026-11-01, bench/sessions/2026-11-01-step20.3-whisper-desk). A first
    // range, under test; flatMin 0 flatMax 1 turn the check off in effect.
    float flatMin = 0.15f;
    float flatMax = 0.40f;
    // 2026-11-05 from the labelled recordings (BandBalance; research/whispernet.md), off by default until heard:
    // timbreCheck 1 makes the timbre module read the low band's excess over the room (whisper <= lowMaxDb) in
    // place of the flatness range.
    uint32_t timbreCheck = 0;  // 0 = flatness (flatMin .. flatMax), 1 = low band (excess <= lowMaxDb)
    float lowMaxDb = 9.0f;
    uint32_t firstCheckMs = 300;   // a running phrase is first judged this far in (the provisional verdicts)
    uint32_t checkEveryMs = 100;   // and again this often
    uint32_t minPhraseMs = 300;    // shorter is no phrase
    uint32_t gapMs = 300;          // syllable gaps up to this are bridged (the detector's release debounce)
    uint32_t maxPhraseMs = 20000;  // longer is no whisper phrase: closed (0: no limit)
};

// The chain at the onset in force (the room may have raised it over v.onset: release and floor keep their ratio to
// the set onset). A phrase is one occurrence: syllable gaps bridged by the release debounce; it is judged while it
// runs (the provisional verdicts), its final verdict only ends it.
inline void makeWhisperChain(const WhisperChainValues& v, float onsetInForce, ScalarTransientConfig& d,
                             InspectionPlan& plan) {
    const float scale = v.onset > 0.0f ? onsetInForce / v.onset : 1.0f;
    const ScalarTransientConfig fresh;  // every field at its default, then the chain's values
    d = fresh;
    d.observedStream = FeatureStreamId::AmpEnvelope;
    d.onsetDetectionThreshold = onsetInForce;
    d.onsetReleaseThreshold = v.release * scale;
    d.cooldownAfterOnsetMs = 0;
    d.maxTransientDurationMs = v.maxPhraseMs;  // a phrase never ending held the room's learning (154 s seen)
    d.minTransientDurationMs = v.minPhraseMs;
    d.minTransientPeakStrength = 0.0f;
    d.releaseDebounceMs = v.gapMs;
    plan = InspectionPlan();  // not a brace list: g++ 13 at -O2 fails inside (an internal error) in this inline body
    InspectionModuleConfig& level = plan.modules[0];
    level.kind = InspectionModuleKind::LevelWindow;
    level.target = InspectionTarget::Loudness;
    level.enabled = true;
    level.minimumStrength = StrengthClass::Strong;
    level.level.floorLevel = v.floor * scale;
    level.level.loudBound = v.loud;
    InspectionModuleConfig& mod = plan.modules[1];
    mod.kind = InspectionModuleKind::Modulation;
    mod.target = InspectionTarget::Modulation;
    mod.enabled = true;
    mod.minimumStrength = StrengthClass::Medium;
    mod.modulation.mediumIndex = v.modulation;
    InspectionModuleConfig& timbre = plan.modules[2];
    timbre.kind = InspectionModuleKind::Flatness;
    timbre.target = InspectionTarget::Timbre;
    timbre.enabled = true;
    timbre.minimumStrength = StrengthClass::Strong;
    if (v.timbreCheck == 1) {
        // The low-band check through the same range module: the median excess, at most lowMaxDb.
        timbre.flatness.stream = FeatureStreamId::LowBandExcess;
        timbre.flatness.minFlatness = -100.0f;
        timbre.flatness.maxFlatness = v.lowMaxDb;
    } else {
        timbre.flatness.minFlatness = v.flatMin;
        timbre.flatness.maxFlatness = v.flatMax;
    }
    plan.count = 3;
}

// The streams a chain records beside the envelope: what the build's FeatureHistory must hold (FEATURE_HISTORY_STREAMS).
inline size_t clapChainStreams() { return 1; }
inline size_t whisperChainStreams() { return 2; }

}  // namespace detection
