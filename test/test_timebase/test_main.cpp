// Time-base math across the 32-bit us wrap (71.6 min) and the 32-bit ms wrap
// (49.7 days); issue #30, docs/refactors/archive/micros-wrap.md. Runs on-device like
// test_analyzer_pass_rules (no host toolchain on the bench machine):
//   pio test -e esp32dev-analyzer -f test_timebase
#include <Arduino.h>
#include <memory>
#include <unity.h>

#include "../../src/app/TimeBase.h"
#include "../../src/app/TimingUtils.h"
#include "../../src/audio/AudioSignal.h"
#include "../../src/detection/features/FeatureHistory.h"
#include "../../src/detection/detectors/scalar/ScalarTransientDetector.h"

#include "../../src/detection/detectors/scalar/ScalarTransientDetector.cpp"
#include "../../src/detection/detectors/scalar/ScalarTransientOccurrence.cpp"
#include "../../src/detection/detectors/scalar/ScalarTransientReport.cpp"
#include "../../src/detection/features/FeatureHistory.cpp"

namespace {

constexpr uint64_t kUsWrap = 0x100000000ULL;               // 2^32 us
constexpr uint64_t kMsWrapUs = 0x100000000ULL * 1000ULL;   // 2^32 ms in us

AudioSamplePacket makePacketUs(uint64_t timeUs64, uint64_t sampleIndex, int level) {
    AudioSamplePacket packet = {};
    packet.sampleIndex = sampleIndex;
    packet.timeUs64 = timeUs64;
    packet.timeMs = timebase::msFromUs64(timeUs64);
    packet.sampleRateHz = 1000UL;
    packet.rawAudioValue = level;
    packet.baselineCorrectedValue = level;
    packet.audioMagnitudeValue = level < 0 ? -level : level;
    packet.level = level;
    packet.smoothedLevel = level;
    packet.valid = true;
    packet.rawHistoryReady = true;
    return packet;
}

// One accepted occurrence: highFrames frames at 100, then lowFrames at 0,
// 1 ms apart, the first at startUs.
bool driveOccurrenceUs(ScalarTransientDetector& detector, uint64_t startUs,
                       unsigned long highFrames, unsigned long lowFrames,
                       detection::Occurrence& out) {
    uint64_t index = 0;
    for (unsigned long i = 0; i < highFrames; ++i, ++index) {
        detector.update(makePacketUs(startUs + index * 1000ULL, index, 100), 100.0f);
    }
    for (unsigned long i = 0; i < lowFrames; ++i, ++index) {
        detector.update(makePacketUs(startUs + index * 1000ULL, index, 0), 0.0f);
    }
    return detector.popOccurrence(out);
}

void configureDetector(ScalarTransientDetector& detector) {
    detector.begin();
    detector.setCooldownAfterOnsetMs(0);
    detector.setReleaseDebounceMs(1);
    detector.setMinTransientDurationMs(0);
    detector.setMaxTransientDurationMs(1000);
    detector.setRequireCarrierQuality(false);
    detector.setRequireMinStrength(false);
}

} // namespace

void test_ms_from_us64_is_continuous_across_us_wrap() {
    // Old code: (uint32_t)us / 1000 jumps from 4294966 back to 0 here.
    TEST_ASSERT_EQUAL_UINT32(4294966UL, timebase::msFromUs64(kUsWrap - 1000ULL));
    TEST_ASSERT_EQUAL_UINT32(4294967UL, timebase::msFromUs64(kUsWrap));
    TEST_ASSERT_EQUAL_UINT32(4294968UL, timebase::msFromUs64(kUsWrap + 1000ULL));
    TEST_ASSERT_EQUAL_UINT32(0UL, timebase::lowUs32(kUsWrap));
}

void test_ms_from_us64_matches_millis_formula() {
    // nowMs() is (unsigned long)(us64 / 1000), exactly Arduino millis().
    const uint64_t us = 123456789012345ULL;
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(us / 1000ULL), timebase::msFromUs64(us));
    // The ms value wraps at 2^32 ms, one ms after 0xFFFFFFFF.
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFUL, timebase::msFromUs64(kMsWrapUs - 1ULL));
    TEST_ASSERT_EQUAL_UINT32(0UL, timebase::msFromUs64(kMsWrapUs));
}

void test_ms_cache_equals_division() {
    timebase::MsFromUs64 cache;
    const uint64_t bases[] = {0ULL, kUsWrap - 50000ULL, kMsWrapUs - 50000ULL};
    for (uint64_t base : bases) {
        cache.reset();
        uint64_t us = base;
        for (uint32_t i = 0; i < 1600; ++i) {
            us += (i & 1U) ? 63ULL : 62ULL; // 16 kHz sample steps
            TEST_ASSERT_EQUAL_UINT32(timebase::msFromUs64(us), cache(us));
        }
        // A jump back and a jump far ahead still convert exactly.
        TEST_ASSERT_EQUAL_UINT32(timebase::msFromUs64(base), cache(base));
        TEST_ASSERT_EQUAL_UINT32(timebase::msFromUs64(base + 7777777ULL), cache(base + 7777777ULL));
    }
}

void test_timing_helpers_across_ms_wrap() {
    const uint32_t before = 0xFFFFFFF0UL;
    const uint32_t after = 0x10UL;
    TEST_ASSERT_TRUE(timing::after(after, before));
    TEST_ASSERT_FALSE(timing::after(before, after));
    TEST_ASSERT_TRUE(timing::atOrAfter(after, before));
    TEST_ASSERT_TRUE(timing::beforeDeadline(before, after));
    TEST_ASSERT_TRUE(timing::atOrBefore(before, after));
    TEST_ASSERT_TRUE(timing::atOrBefore(after, after));
    TEST_ASSERT_EQUAL_INT32(0x20, timing::delta(after, before));
    TEST_ASSERT_EQUAL_INT32(-0x20, timing::delta(before, after));
    TEST_ASSERT_EQUAL_UINT32(0x20UL, timing::elapsedOrZero(after, before));
    TEST_ASSERT_EQUAL_UINT32(0UL, timing::elapsedOrZero(before, after));
    TEST_ASSERT_TRUE(timing::inWindow(0x0UL, before, after));
    TEST_ASSERT_TRUE(timing::inWindow(before, before, after));
    TEST_ASSERT_TRUE(timing::inWindow(after, before, after));
    TEST_ASSERT_FALSE(timing::inWindow(after + 1UL, before, after));
    TEST_ASSERT_FALSE(timing::inWindow(before - 1UL, before, after));
}

void test_now_ms_agrees_with_millis_when_offset_is_zero() {
    if (timebase::kOffsetUs != 0) {
        TEST_IGNORE_MESSAGE("TIMEBASE_OFFSET_US set: nowMs() is millis() + offset by design");
    }
    const unsigned long a = millis();
    const unsigned long t = timebase::nowMs();
    const unsigned long b = millis();
    TEST_ASSERT_TRUE(timing::atOrAfter(t, a));
    TEST_ASSERT_TRUE(timing::atOrBefore(t, b));
}

void test_feature_history_window_across_ms_wrap() {
    detection::FeatureStreamId stream = detection::FeatureStreamId::AmpMagnitude;
    std::unique_ptr<detection::FeatureHistory> history(new detection::FeatureHistory());
    history->setActiveStreams(&stream, 1);
    for (uint32_t t = 0xFFFFFFF0UL; t != 0x11UL; ++t) {
        history->record(stream, t, 10.0f);
    }
    const auto window = history->getWindow(stream, 0xFFFFFFF8UL, 0x8UL, 0x10UL);
    TEST_ASSERT_TRUE(window.hasValues);
    TEST_ASSERT_EQUAL_UINT32(17UL, window.durationMs);
    TEST_ASSERT_EQUAL_UINT32(0UL, window.leftMissingMs);
    TEST_ASSERT_EQUAL_UINT32(0UL, window.rightMissingMs);
    TEST_ASSERT_FALSE(window.requestedFutureAtInspection);
    TEST_ASSERT_TRUE(window.coverageComplete);
    TEST_ASSERT_EQUAL_UINT32(17UL, static_cast<uint32_t>(window.bucketCount));
}

void test_scalar_occurrence_across_us_wrap() {
    ScalarTransientDetector detector;
    configureDetector(detector);
    detection::Occurrence occurrence = {};
    const uint64_t startUs = kUsWrap - 3000ULL; // onset 3 ms before the us wrap
    TEST_ASSERT_TRUE(driveOccurrenceUs(detector, startUs, 6, 25, occurrence));
    TEST_ASSERT_TRUE(occurrence.present);
    // Onset at one of the six high frames, on the packet ms axis.
    TEST_ASSERT_TRUE(timing::inWindow(occurrence.startMs, timebase::msFromUs64(startUs), timebase::msFromUs64(startUs) + 5UL));
    TEST_ASSERT_TRUE(timing::atOrAfter(occurrence.endMs, occurrence.startMs));
    TEST_ASSERT_TRUE(timing::delta(occurrence.endMs, occurrence.startMs) < 100);
}

void test_scalar_occurrence_across_ms_wrap() {
    ScalarTransientDetector detector;
    configureDetector(detector);
    detection::Occurrence occurrence = {};
    const uint64_t startUs = kMsWrapUs - 3000ULL; // onset 3 ms before the ms wrap
    TEST_ASSERT_TRUE(driveOccurrenceUs(detector, startUs, 6, 25, occurrence));
    TEST_ASSERT_TRUE(occurrence.present);
    TEST_ASSERT_TRUE(timing::inWindow(occurrence.startMs, 0xFFFFFFFDUL, 0x2UL));
    TEST_ASSERT_TRUE(timing::atOrAfter(occurrence.endMs, occurrence.startMs));
    TEST_ASSERT_TRUE(timing::delta(occurrence.endMs, occurrence.startMs) < 100);
}

void setup() {
    delay(100);
    UNITY_BEGIN();
    RUN_TEST(test_ms_from_us64_is_continuous_across_us_wrap);
    RUN_TEST(test_ms_from_us64_matches_millis_formula);
    RUN_TEST(test_ms_cache_equals_division);
    RUN_TEST(test_timing_helpers_across_ms_wrap);
    RUN_TEST(test_now_ms_agrees_with_millis_when_offset_is_zero);
    RUN_TEST(test_feature_history_window_across_ms_wrap);
    RUN_TEST(test_scalar_occurrence_across_us_wrap);
    RUN_TEST(test_scalar_occurrence_across_ms_wrap);
    UNITY_END();
}

void loop() {}
