/*
EnvelopeRuntime's provisional verdicts on the host (ECHO-002): with setProvisional on, the open candidate is
judged by the plan (Whispernet's level window and modulation) once it is old enough and every interval after, while
the phrase runs, so the gate can open before the phrase ends. A whispered phrase gets a valid provisional verdict
within ~0.3 s, a steady noise only failing ones, and a clap none (its tail is in the release debounce). The chain
is the shell's (detection/EchoProfiles.h) on the whisper board's history geometry (64 x 8 ms x 2, pinned below).
Runs in native-test-behavior.
*/
// The whisper board's FeatureHistory geometry (platformio.ini, esp32dev-whisper), before the header reads the macros.
#define FEATURE_HISTORY_BINS 64
#define FEATURE_HISTORY_BIN_MS 8
#define FEATURE_HISTORY_STREAMS 2
// The echo modes' inspection modules are built only where ECHO_INSPECTION is defined (EchoInspection.cpp).
#define ECHO_INSPECTION

#include <unity.h>

#include "../../src/detection/EchoProfiles.h"
#include "../../src/detection/EnvelopeRuntime.h"

#include "../../src/detection/EnvelopeRuntime.cpp"
#include "../../src/detection/detectors/scalar/ScalarTransientDetector.cpp"
#include "../../src/detection/detectors/scalar/ScalarTransientOccurrence.cpp"
#include "../../src/detection/detectors/scalar/ScalarTransientReport.cpp"
#include "../../src/detection/features/FeatureHistory.cpp"
#include "../../src/detection/inspection/EchoInspection.cpp"
#include "../../src/detection/inspection/OccurrenceInspector.cpp"
#include "../../src/detection/inspection/VerdictProjection.cpp"

using namespace detection;

namespace {

// The shell's chain at the synthetic set's levels (onset 1500 / release 1000 / floor 1000, the loud bound out of
// the way), as native-whisperreplay runs it on that set.
void configureWhisper(EnvelopeRuntime& rt) {
    WhisperChainValues v;
    v.onset = 1500.0f;
    v.release = 1000.0f;
    v.floor = 1000.0f;
    v.loud = 20000.0f;
    ScalarTransientConfig d;
    InspectionPlan plan;
    makeWhisperChain(v, v.onset, d, plan);
    rt.configure(d, plan);
    rt.setProvisional(v.firstCheckMs, v.checkEveryMs);
    rt.reset();
}

struct Tally {
    size_t provisionalValid = 0;
    size_t provisionalFailed = 0;
    unsigned long firstValidAfterMs = 0;
    size_t finalValid = 0;
    size_t finals = 0;
};

// 4 s of blocks of 128 samples at 24 kHz (~5.3 ms), the level from the shape, then silence.
Tally run(float (*level)(unsigned long)) {
    static EnvelopeRuntime rt;  // its history is large: not on the stack
    configureWhisper(rt);
    Tally t;
    for (uint64_t us = 0; us < 4000000ULL; us += 5333ULL) {
        const unsigned long ms = static_cast<unsigned long>(us / 1000ULL);
        if (level(ms) >= 1500.0f) {  // the flatness per loud block, as the shell records it: a whisper's
            rt.recordFeature(FeatureStreamId::SpectralFlatness, static_cast<uint32_t>(ms), 0.3f);
        }
        rt.onBlock(level(ms), us, static_cast<uint32_t>(ms));
        EnvelopeRuntime::Result r;
        while (rt.popResult(r)) {
            if (!r.provisional) {
                ++t.finals;
                t.finalValid += r.verdict.valid ? 1 : 0;
            } else if (r.verdict.valid) {
                if (t.provisionalValid++ == 0) {
                    t.firstValidAfterMs = r.inspected.occurrence.durationMs;
                }
            } else {
                ++t.provisionalFailed;
            }
        }
    }
    return t;
}

}  // namespace

void setUp() {}
void tearDown() {}

// Syllables at 4 Hz between 9000 and 1200 from 100 to 2100 ms: the gate may open within ~0.4 s, the phrase passes.
void test_a_whisper_is_known_while_it_runs() {
    const Tally t =
        run([](unsigned long ms) { return ms < 100 || ms > 2100 ? 0.0f : ((ms / 125) % 2 == 0 ? 9000.0f : 1200.0f); });
    TEST_ASSERT_TRUE(t.provisionalValid > 0);
    TEST_ASSERT_TRUE(t.firstValidAfterMs >= 300 && t.firstValidAfterMs < 450);
    TEST_ASSERT_EQUAL_UINT32(1, t.finals);
    TEST_ASSERT_EQUAL_UINT32(1, t.finalValid);
}

// A fan, steady at 5000: judged while it runs, never valid.
void test_a_steady_noise_never_opens() {
    const Tally t = run([](unsigned long ms) { return ms < 100 || ms > 2100 ? 0.0f : 5000.0f; });
    TEST_ASSERT_TRUE(t.provisionalFailed > 0);
    TEST_ASSERT_EQUAL_UINT32(0, t.provisionalValid);
}

// A clap, 30 ms at 30000: its candidate is in the release debounce from then on, no provisional verdict at all.
void test_a_clap_gets_no_provisional_verdict() {
    const Tally t = run([](unsigned long ms) { return ms >= 1000 && ms < 1030 ? 30000.0f : 0.0f; });
    TEST_ASSERT_EQUAL_UINT32(0, t.provisionalValid + t.provisionalFailed);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_whisper_is_known_while_it_runs);
    RUN_TEST(test_a_steady_noise_never_opens);
    RUN_TEST(test_a_clap_gets_no_provisional_verdict);
    return UNITY_END();
}
