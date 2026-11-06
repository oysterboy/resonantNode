/*
Whispernet's inspection on the host (ECHO-002): the LevelWindow, Modulation and Flatness modules over a
phrase's span, the plan as the shell builds it (detection/EchoProfiles.h, at the synthetic set's levels); a whisper
passes, a fan fails on modulation, a loud phrase on the level window, rustling on the timbre. The history is the
whisper board's (esp32dev-whisper: 64 bins x 8 ms x 2 streams, pinned below), so the P90, the median and
the modulation grid read the bins the board averages, not the Node's 1 ms ones. Runs in native-test-behavior.
*/
// The whisper board's FeatureHistory geometry (platformio.ini, esp32dev-whisper), before the header reads the macros.
#define FEATURE_HISTORY_BINS 64
#define FEATURE_HISTORY_BIN_MS 8
#define FEATURE_HISTORY_STREAMS 2
// The echo modes' inspection modules are built only where ECHO_INSPECTION is defined (EchoInspection.cpp).
#define ECHO_INSPECTION

#include <unity.h>

#include "../../src/detection/EchoProfiles.h"
#include "../../src/detection/features/FeatureHistory.h"
#include "../../src/detection/inspection/OccurrenceInspector.h"
#include "../../src/detection/inspection/VerdictProjection.h"

#include "../../src/detection/features/FeatureHistory.cpp"
#include "../../src/detection/inspection/EchoInspection.cpp"
#include "../../src/detection/inspection/OccurrenceInspector.cpp"
#include "../../src/detection/inspection/VerdictProjection.cpp"

using namespace detection;

namespace {

// The chain at the synthetic set's levels (synthwhisper.py reads ~16 dB above the desk's mic): the shipped values
// with onset 1500 / release 1000 / floor 1000 and the loud bound out of the way; the flatness range as shipped.
WhisperChainValues syntheticValues() {
    WhisperChainValues v;
    v.onset = 1500.0f;
    v.release = 1000.0f;
    v.floor = 1000.0f;
    v.loud = 20000.0f;
    return v;
}

InspectionPlan whisperPlan() {
    ScalarTransientConfig d;
    InspectionPlan plan;
    makeWhisperChain(syntheticValues(), syntheticValues().onset, d, plan);
    return plan;
}

Occurrence occurrence(unsigned long startMs, unsigned long durationMs) {
    Occurrence o = {};
    o.present = true;
    o.valid = true;
    o.occurrenceType = OccurrenceType::Scalar;
    o.startMs = startMs;
    o.durationMs = durationMs;
    o.releaseMs = startMs + durationMs;
    o.strength = 9000.0f;
    return o;
}

FeatureHistory& history() {
    static FeatureHistory h;  // the whisper board's size (64 x 8 ms x 2)
    const FeatureStreamId streams[2] = {FeatureStreamId::AmpEnvelope, FeatureStreamId::SpectralFlatness};
    h.reset();
    h.setActiveStreams(streams, 2);
    return h;
}

// A phrase from 10000 to 11000 ms: the level from the shape, recorded every 5 ms as a 24 kHz block would, and the
// flatness per block as the shell records it for a loud block (a whisper's 0.3 unless given).
OccurrenceVerdict judgePhrase(float (*level)(unsigned long), InspectedOccurrence& inspected, float flatness = 0.3f) {
    FeatureHistory& h = history();
    for (unsigned long t = 9900; t <= 11000; t += 5) {
        h.record(FeatureStreamId::AmpEnvelope, t, level(t));
        h.record(FeatureStreamId::SpectralFlatness, t, flatness);
    }
    const Occurrence occ = occurrence(10000, 1000);
    OccurrenceInspector inspector;
    inspector.configure(whisperPlan());
    inspected = inspector.inspectWithHistory(occ, &h, 11000);
    OccurrenceVerdict v;
    makeVerdict(inspected, whisperPlan(), v);
    return v;
}

}  // namespace

void setUp() {}
void tearDown() {}

// Syllables at 4 Hz between 9000 and 1000: inside the level window, strongly modulated, a whisper's timbre.
void test_a_whisper_passes() {
    InspectedOccurrence in;
    const OccurrenceVerdict v = judgePhrase([](unsigned long t) { return (t / 125) % 2 == 0 ? 9000.0f : 1000.0f; }, in);
    TEST_ASSERT_TRUE(in.occurrence.level.measured);
    TEST_ASSERT_EQUAL(StrengthClass::Strong, in.occurrence.level.levelClass);
    TEST_ASSERT_TRUE(in.occurrence.modulation.measured);
    TEST_ASSERT_TRUE(in.occurrence.modulation.index > 0.5f);
    TEST_ASSERT_EQUAL(StrengthClass::Strong, in.occurrence.timbre.timbreClass);
    TEST_ASSERT_TRUE(v.valid);
}

// A fan: steady at 5000, inside the level window, no movement at syllable rate.
void test_a_steady_noise_fails_on_modulation() {
    InspectedOccurrence in;
    const OccurrenceVerdict v = judgePhrase([](unsigned long) { return 5000.0f; }, in);
    TEST_ASSERT_EQUAL(StrengthClass::Strong, in.occurrence.level.levelClass);
    TEST_ASSERT_EQUAL(StrengthClass::None, in.occurrence.modulation.modulationClass);
    TEST_ASSERT_FALSE(v.valid);
    TEST_ASSERT_EQUAL(InspectionTarget::Modulation, v.firstFailedRequirementTarget);
}

// Speech or the own loop growing: the same syllables, too loud (P90 above the bound), the gate stays shut.
void test_a_loud_phrase_fails_on_the_level_window() {
    InspectedOccurrence in;
    const OccurrenceVerdict v =
        judgePhrase([](unsigned long t) { return (t / 125) % 2 == 0 ? 30000.0f : 3000.0f; }, in);
    TEST_ASSERT_EQUAL(StrengthClass::None, in.occurrence.level.levelClass);
    TEST_ASSERT_FALSE(v.valid);
    TEST_ASSERT_EQUAL(InspectionTarget::Loudness, v.firstFailedRequirementTarget);
}

// Rustling: the syllable shape of a whisper, the flatness of noise (0.5, above the range): the timbre fails.
void test_rustling_fails_on_the_timbre() {
    InspectedOccurrence in;
    const OccurrenceVerdict v =
        judgePhrase([](unsigned long t) { return (t / 125) % 2 == 0 ? 9000.0f : 1000.0f; }, in, 0.5f);
    TEST_ASSERT_TRUE(in.occurrence.timbre.measured);
    TEST_ASSERT_EQUAL(StrengthClass::None, in.occurrence.timbre.timbreClass);
    TEST_ASSERT_FALSE(v.valid);
    TEST_ASSERT_EQUAL(InspectionTarget::Timbre, v.firstFailedRequirementTarget);
}

// The phrase is judged once its span has passed.
void test_whisper_window_complete_at_the_end() {
    OccurrenceInspector inspector;
    inspector.configure(whisperPlan());
    TEST_ASSERT_EQUAL_UINT32(11000, inspector.windowCompleteAtMs(occurrence(10000, 1000)));
}

// The timbre module alone: the median of the span's flatness values between the bounds; too few values: Unknown.
void test_flatness_window() {
    FeatureHistory& h = history();
    for (unsigned long t = 10000; t <= 11000; t += 16) {
        h.record(FeatureStreamId::SpectralFlatness, t, t < 10500 ? 0.12f : 0.15f);  // a whisper's
    }
    InspectionPlan plan = InspectionPlan{};
    plan.modules[0].kind = InspectionModuleKind::Flatness;
    plan.modules[0].target = InspectionTarget::Timbre;
    plan.modules[0].enabled = true;
    plan.modules[0].minimumStrength = StrengthClass::Strong;
    plan.modules[0].flatness.minFlatness = syntheticValues().flatMin;
    plan.modules[0].flatness.maxFlatness = syntheticValues().flatMax;
    plan.count = 1;
    const Occurrence occ = occurrence(10000, 1000);
    OccurrenceInspector inspector;
    inspector.configure(plan);
    InspectedOccurrence in = inspector.inspectWithHistory(occ, &h, 11000);
    TEST_ASSERT_TRUE(in.occurrence.timbre.measured);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.15f, in.occurrence.timbre.flatness);  // the last 512 ms
    TEST_ASSERT_EQUAL(StrengthClass::Strong, in.occurrence.timbre.timbreClass);
    OccurrenceVerdict v;
    makeVerdict(in, plan, v);
    TEST_ASSERT_TRUE(v.valid);

    history();
    h.record(FeatureStreamId::SpectralFlatness, 10900, 0.1f);  // one loud block only
    in = inspector.inspectWithHistory(occ, &h, 11000);
    TEST_ASSERT_FALSE(in.occurrence.timbre.measured);
    TEST_ASSERT_EQUAL(StrengthClass::Unknown, in.occurrence.timbre.timbreClass);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_whisper_passes);
    RUN_TEST(test_a_steady_noise_fails_on_modulation);
    RUN_TEST(test_a_loud_phrase_fails_on_the_level_window);
    RUN_TEST(test_rustling_fails_on_the_timbre);
    RUN_TEST(test_whisper_window_complete_at_the_end);
    RUN_TEST(test_flatness_window);
    return UNITY_END();
}
