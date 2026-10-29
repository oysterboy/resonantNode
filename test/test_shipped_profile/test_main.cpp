/*
Step 7b (audit 4.1, 4.3): the shipped scalar profile on the board.

test_analyzer_pass_rules and test_timebase drive the scalar detector with every shipped gate off.
This test runs makeTonalPulseScalarProfile() unchanged, through the real chain the Node runs
(AudioSignal -> FreqBandStream -> DetectionRuntime, float Goertzel on the ESP32), over the
captures the host suite (test/test_replay_corpus) asserts on, with the same expectations. A
board/host difference in the frequency path that flips a verdict fails here or there, not in
the field. The chain is fed at sample timestamps, faster than real time; detection reads only
those (the wall clock feeds profiling counters).

The PCM is in fixtures.h, generated from test/fixtures by tools/bench/fixture2header.py; all
four are mode=both or .wav captures, so they hold what AudioSignal received and are fed
without a preprocessor (as hostreplay::replay does for them).

Board (any D-AMP board; no audio I/O): `pio test -e esp32dev-analyzer -f test_shipped_profile
--upload-port COMn --without-testing`, then reset the board and read the port. The envs set
monitor_dtr/rts = 0, so the test runner's port open does not reset the board and misses the output.
Flash the board's own build back afterwards.
Host: `pio test -e native-test-behavior` (no src build; this file pulls the chain in itself).
*/
#include <Arduino.h>
#include <memory>
#include <unity.h>

#include "../../src/app/RuntimeDefaults.h"
#include "../../src/audio/AudioSignal.h"
#include "../../src/audio/AudioSource.h"
#include "../../src/detection/DetectionProfile.h"
#include "../../src/detection/DetectionRuntime.h"
#include "../../src/detection/evaluation/OccurrenceVerdict.h"
#include "../../src/detection/features/FreqBandStream.h"
#include "../../src/detection/features/FrequencyMeasurementPacketBuilder.h"

// test_build_src is off for the ESP envs: pull the chain into the test build.
#include "../../src/audio/AudioSignal.cpp"
#include "../../src/detection/DetectionRuntime.cpp"
#include "../../src/detection/DetectionDiagnostics.cpp"  // the Analyzer build (ANALYZER_MODE) links its recorder
#include "../../src/detection/detectors/DetectorNames.cpp"
#include "../../src/detection/detectors/scalar/ScalarTransientDetector.cpp"
#include "../../src/detection/detectors/scalar/ScalarTransientOccurrence.cpp"
#include "../../src/detection/detectors/scalar/ScalarTransientReport.cpp"
#include "../../src/detection/inspection/VerdictProjection.cpp"
#include "../../src/detection/features/FeatureHistory.cpp"
#include "../../src/detection/features/FreqBandStream.cpp"
#include "../../src/detection/field/FieldStateTracker.cpp"
#include "../../src/detection/inspection/OccurrenceInspector.cpp"

#include "fixtures.h"

namespace {

class ArrayAudioSource : public AudioSource {
public:
    ArrayAudioSource(const int32_t* pcm, size_t count, uint32_t rate)
        : _pcm(pcm),
          _count(count),
          _rate(rate) {}
    void begin() override {}
    bool available() override { return _index < _count; }
    bool readSample(int& sample, uint64_t& sampleTimeUs64) override {
        if (_index >= _count) {
            return false;
        }
        sample = _pcm[_index];
        sampleTimeUs64 = timeUs(_index);
        ++_index;
        return true;
    }
    uint32_t sampleRateHz() const override { return _rate; }
    uint64_t timeUs(size_t index) const { return (static_cast<uint64_t>(index) * 1000000ULL) / _rate; }

private:
    const int32_t* _pcm;
    size_t _count;
    uint32_t _rate;
    size_t _index = 0;
};

struct Fixture {
    const int32_t* pcm;
    size_t samples;
    uint32_t rateHz;
    bool hasBaseline;
    float baseline;
};

#define FIXTURE(name) \
    Fixture { \
        k##name##Pcm, k##name##Samples, k##name##RateHz, k##name##HasBaseline, k##name##Baseline \
    }

struct Band {
    unsigned long lowHz;
    unsigned long highHz;
};

constexpr Band kSingleTarget{0, 0};
constexpr Band kNetworkBand{runtime::kDefaultNetworkBandLowHz, runtime::kDefaultNetworkBandHighHz};

struct Outcome {
    unsigned accepted = 0;
    unsigned rejected = 0;
    detection::OccurrenceVerdict first;
    bool haveFirst = false;
};

// The chain is large (FeatureHistory alone ~18.6 KB, AudioSignal holds its sample history) and the
// test runs on the 16 KB loop stack, where FeatureHistory::reset() also puts a StreamBuffer
// temporary: the runtime and band stream are static, AudioSignal and the profile on the heap.
FreqBandStream gBand;
detection::DetectionRuntime gRuntime;

// hostreplay::replay() for one capture: the same conditioning, the same order of calls, 300 ms
// of silence after the capture so the last occurrence closes, verdicts before skipMs ignored.
Outcome replay(const Fixture& fx, Band band, unsigned long skipMs = 0) {
    ArrayAudioSource source(fx.pcm, fx.samples, fx.rateHz);
    std::unique_ptr<AudioSignal> audioSignalOwner(new AudioSignal(source));
    AudioSignal& audioSignal = *audioSignalOwner;
    audioSignal.begin(false);
    audioSignal.setSmoothingFactor(0.5f);
    audioSignal.setBaselineUpdateFactor(0.005f);
    audioSignal.setBaselineTrackingQuietThreshold(20);
    if (fx.hasBaseline) {
        audioSignal.seedBaseline(fx.baseline);
    } else {
        audioSignal.rebase();
    }

    gBand.resetState();
    gBand.setSampleRateHz(fx.rateHz);
    gBand.setTargetFrequencyHz(runtime::kDefaultChirpFrequencyHz);
    gBand.setBand(band.lowHz, band.highHz);

    std::unique_ptr<detection::DetectionProfile> profileOwner(
        new detection::DetectionProfile(detection::makeTonalPulseScalarProfile()));
    const detection::DetectionProfile& profile = *profileOwner;
    gRuntime.resetState();
    gRuntime.setDetectorSelection(profile.detectorSelection);
    gRuntime.setScalarTransientConfig(profile.scalarTransient);
    gRuntime.setInspectionPlan(profile.inspectionPlan);
    gRuntime.setFieldStateConfig(profile.fieldStateConfig);
    gRuntime.setVerdictQueueEnabled(true);

    Outcome out;
    const size_t total = fx.samples + (300UL * fx.rateHz) / 1000UL;
    for (size_t i = 0; i < total; ++i) {
        const int sample = i < fx.samples ? fx.pcm[i] : 0;
        const uint64_t us = source.timeUs(i);
        AudioSamplePacket packet;
        audioSignal.update(sample, us, packet, false);
        gBand.observeCenteredSample(packet.baselineCorrectedValue, packet.timeMs);
        const detection::FrequencyBandMeasurementPacket evidence =
            detection::buildFrequencyMeasurementPacket(gBand, packet);
        gRuntime.observeFrame(packet, evidence, packet.timeMs);
        detection::OccurrenceVerdict verdict;
        while (gRuntime.popOccurrenceVerdict(verdict)) {
            if (verdict.primaryStartMs < skipMs) {
                continue;
            }
            verdict.accepted ? ++out.accepted : ++out.rejected;
            if (!out.haveFirst) {
                out.first = verdict;
                out.haveFirst = true;
            }
        }
        if ((i & 1023U) == 0) {
            yield();  // a capture is ~5k samples; keep the task watchdog fed
        }
    }
    return out;
}

}  // namespace

void setUp() {}
void tearDown() {}

// Single target at 3,200 Hz: accepted once, inside the profile's 85..300 ms window.
void test_chirp_is_accepted_once_on_the_single_target() {
    const Outcome r = replay(FIXTURE(Chirp), kSingleTarget);
    TEST_ASSERT_EQUAL_UINT32(1, r.accepted);
    TEST_ASSERT_EQUAL_UINT32(0, r.rejected);
    TEST_ASSERT_TRUE(r.first.primaryDurationMs >= 85 && r.first.primaryDurationMs <= 300);
}

// Band mode: the on-bin 3,200 Hz chirp is rejected by shape (one bin; DET-015, until 2026-10-26 accepted).
void test_on_bin_tone_is_rejected_by_shape_in_band_mode() {
    const Outcome r = replay(FIXTURE(Chirp), kNetworkBand);
    TEST_ASSERT_EQUAL_UINT32(0, r.accepted);
    TEST_ASSERT_EQUAL_UINT32(1, r.rejected);
}

// Band mode: the off-bin 3,300 Hz tone fails the contrast gate, the hard threshold on a P75
// that a few units of float drift could flip (audit 4.1).
void test_off_bin_tone_fails_the_contrast_gate_in_band_mode() {
    const Outcome r = replay(FIXTURE(Tone3300), kNetworkBand);
    TEST_ASSERT_EQUAL_UINT32(0, r.accepted);
    TEST_ASSERT_EQUAL_UINT32(1, r.rejected);
    TEST_ASSERT_EQUAL(detection::InspectionTarget::Contrast, r.first.firstFailedRequirementTarget);
}

// The network signal as shipped (5 x 25 ms, gap 10, on the band bins): one occurrence, the
// gaps bridged, both inspectors passed (coverage, island, gap and the duration window on).
void test_hop_train_is_accepted_once_in_band_mode() {
    const Outcome r = replay(FIXTURE(Hop), kNetworkBand);
    TEST_ASSERT_EQUAL_UINT32(1, r.accepted);
    TEST_ASSERT_EQUAL_UINT32(0, r.rejected);
    TEST_ASSERT_TRUE_MESSAGE(r.first.primaryDurationMs >= 150 && r.first.primaryDurationMs <= 300,
                             "hop train duration outside 150..300 ms");
}

// The hop train is not heard on the single 3,200 Hz target: four of its five steps are elsewhere.
void test_hop_train_is_not_heard_on_the_single_target() {
    const Outcome r = replay(FIXTURE(Hop), kSingleTarget);
    TEST_ASSERT_EQUAL_UINT32(0, r.accepted);
}

// Speech from the phone radio (after its 20 ms marker tone) is not accepted in band mode.
void test_radio_speech_is_not_accepted() {
    const Outcome r = replay(FIXTURE(Speech), kNetworkBand, 100);
    TEST_ASSERT_EQUAL_UINT32(0, r.accepted);
}

// Step 8b item 3 (audit 3.7): a continuous in-band tone is closed as too long once it passes
// max duration + debounce (460 ms on the shipped profile since 2026-10-29), not when it ends, and no new
// candidate opens until the signal drops below release. A normal pulse afterwards is accepted.
namespace {
ScalarTransientDetector gScalar;

void feedScalar(unsigned long& ms, unsigned long count, float value) {
    for (unsigned long i = 0; i < count; ++i, ++ms) {
        AudioSamplePacket packet = {};
        packet.timeMs = ms;
        packet.timeUs64 = static_cast<uint64_t>(ms) * 1000ULL;
        packet.valid = true;
        gScalar.update(packet, value);
    }
}
}  // namespace

void configureShippedScalar() {
    const detection::ScalarTransientConfig c = detection::makeTonalPulseScalarProfile().scalarTransient;
    gScalar.resetState();
    gScalar.resetSelectedRejectSummary();
    gScalar.resetAcceptedOccurrenceSummary();
    gScalar.setConfig(c);
}

void test_continuous_tone_is_closed_at_max_duration() {
    configureShippedScalar();
    unsigned long ms = 1000;
    feedScalar(ms, 100, 0.0f);
    feedScalar(ms, 480, 5000.0f);  // 480 ms into a tone that goes on
    TEST_ASSERT_EQUAL_UINT32(1, gScalar.latestReport().scalar.aggregates.tooLongCount);
    const uint32_t rejectsAfterClose = gScalar.selectedRejectGeneration();
    feedScalar(ms, 600, 5000.0f);  // the tone continues: no new candidate, no new reject
    TEST_ASSERT_EQUAL_UINT32(rejectsAfterClose, gScalar.selectedRejectGeneration());
    detection::Occurrence occurrence;
    TEST_ASSERT_FALSE(gScalar.popOccurrence(occurrence));

    feedScalar(ms, 200, 0.0f);     // the tone ends
    feedScalar(ms, 120, 5000.0f);  // a 120 ms pulse
    feedScalar(ms, 100, 0.0f);
    TEST_ASSERT_TRUE(gScalar.popOccurrence(occurrence));
    TEST_ASSERT_TRUE(occurrence.valid);
    TEST_ASSERT_TRUE(occurrence.durationMs >= 115 && occurrence.durationMs <= 135);
}

// Step 8b item 7 (#32 13 / B11, decisions/2026-10-13-one-reject-event-per-new-best-reject.md):
// every reject counts in the report's rejected count; only a new best reject (longer, then
// stronger) bumps the generation that makes a reject event. 40, 30 and 60 ms pulses, all too
// short: 3 rejects, 2 new best (the 30 ms one does not beat the 40 ms one).
void test_every_reject_counts_only_a_new_best_makes_an_event() {
    configureShippedScalar();
    unsigned long ms = 1000;
    feedScalar(ms, 100, 0.0f);
    const uint32_t generationBefore = gScalar.selectedRejectGeneration();
    const unsigned long pulses[] = {40, 30, 60};
    for (unsigned long pulseMs : pulses) {
        feedScalar(ms, pulseMs, 5000.0f);
        feedScalar(ms, 150, 0.0f);
    }
    TEST_ASSERT_EQUAL_UINT32(3, gScalar.latestReport().aggregates.rejectedCount);
    TEST_ASSERT_EQUAL_UINT32(generationBefore + 2, gScalar.selectedRejectGeneration());
    TEST_ASSERT_EQUAL_UINT32(60, gScalar.latestReport().selectedReject.durationMs);
}

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(test_chirp_is_accepted_once_on_the_single_target);
    RUN_TEST(test_on_bin_tone_is_rejected_by_shape_in_band_mode);
    RUN_TEST(test_off_bin_tone_fails_the_contrast_gate_in_band_mode);
    RUN_TEST(test_hop_train_is_accepted_once_in_band_mode);
    RUN_TEST(test_hop_train_is_not_heard_on_the_single_target);
    RUN_TEST(test_radio_speech_is_not_accepted);
    RUN_TEST(test_continuous_tone_is_closed_at_max_duration);
    RUN_TEST(test_every_reject_counts_only_a_new_best_makes_an_event);
    return UNITY_END();
}

// The same file runs on the host (env native-test-behavior, CI) and on the board: one set of
// inputs and expectations for both float Goertzel builds.
#if defined(ARDUINO_ARCH_ESP32)
void setup() {
    delay(2000);  // the test runner opens the port after reset
    runTests();
}

void loop() {}
#else
int main(int, char**) {
    return runTests();
}
#endif
