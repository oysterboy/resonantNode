#include <Arduino.h>
#include <initializer_list>
#include <unity.h>

#include "../../src/audio/AudioSignal.h"
#include "../../src/detection/analyzer/AnalyzerPassRules.h"
#include "../../src/detection/features/FeatureHistory.h"
#include "../../src/detection/detectors/scalar/ScalarTransientDetector.h"

// Pull the detector implementation into the test build so the regression can
// run without changing the firmware build configuration.
#include "../../src/detection/detectors/scalar/ScalarTransientDetector.cpp"
#include "../../src/detection/detectors/scalar/ScalarTransientOccurrence.cpp"
#include "../../src/detection/detectors/scalar/ScalarTransientReport.cpp"
#include "../../src/detection/features/FeatureHistory.cpp"

namespace {

constexpr float kQuantileEpsilon = 0.0001f;

AudioSamplePacket makePacket(unsigned long timeMs, uint64_t sampleIndex, int level) {
    AudioSamplePacket packet = {};
    packet.sampleIndex = sampleIndex;
    packet.timeUs = timeMs * 1000UL;
    packet.timeMs = timeMs;
    packet.sampleRateHz = 1000UL;
    packet.rawAudioValue = level;
    packet.baselineCorrectedValue = level;
    packet.audioMagnitudeValue = level < 0 ? -level : level;
    packet.level = level;
    packet.smoothedLevel = level;
    packet.baseline = 0.0f;
    packet.valid = true;
    packet.rawHistoryReady = true;
    packet.overflowDuringBlock = false;
    return packet;
}

void recordSeries(detection::FeatureHistory& history,
                  std::initializer_list<float> values,
                  detection::FeatureStreamId stream = detection::FeatureStreamId::AmpMagnitude) {
    unsigned long timeMs = 0;
    for (float value : values) {
        history.record(stream, timeMs++, value);
    }
}

void assertQuantileOrdering(const detection::ScalarWindow& window) {
    TEST_ASSERT_TRUE_MESSAGE(window.min <= window.median + kQuantileEpsilon, "median should be >= min");
    TEST_ASSERT_TRUE_MESSAGE(window.median <= window.p75 + kQuantileEpsilon, "p75 should be >= median");
    TEST_ASSERT_TRUE_MESSAGE(window.p75 <= window.p90 + kQuantileEpsilon, "p90 should be >= p75");
    TEST_ASSERT_TRUE_MESSAGE(window.p90 <= window.max + kQuantileEpsilon, "p90 should be <= max");
}

void driveAcceptedOccurrence(ScalarTransientDetector& detector,
                             unsigned long startMs,
                             unsigned long highFrames,
                             unsigned long lowFrames,
                             detection::Occurrence& outOccurrence,
                             detection::DetectorReport& outReport) {
    for (unsigned long i = 0; i < highFrames; ++i) {
        const unsigned long timeMs = startMs + i;
        const auto packet = makePacket(timeMs, timeMs, 100);
        detector.update(packet, 100.0f);
    }

    for (unsigned long i = 0; i < lowFrames; ++i) {
        const unsigned long timeMs = startMs + highFrames + i;
        const auto packet = makePacket(timeMs, timeMs, 0);
        detector.update(packet, 0.0f);
    }

    TEST_ASSERT_TRUE_MESSAGE(detector.popOccurrence(outOccurrence), "expected accepted occurrence");
    detector.buildReport(outReport, startMs + highFrames + lowFrames);
}

} // namespace

void test_feature_history_empty_window() {
    detection::FeatureHistory history;
    const auto window = history.getWindow(
        detection::FeatureStreamId::AmpMagnitude,
        0,
        10,
        10);

    TEST_ASSERT_FALSE(window.hasValues);
    TEST_ASSERT_FALSE(window.valid);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, window.mean);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, window.rms);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, window.median);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, window.p75);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, window.p90);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, window.trimmedMean);
}

void test_feature_history_single_value_quantiles() {
    detection::FeatureHistory history;
    recordSeries(history, {42.0f});
    const auto window = history.getWindow(detection::FeatureStreamId::AmpMagnitude, 0, 0, 0);

    TEST_ASSERT_TRUE(window.hasValues);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, window.min);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, window.max);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, window.median);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, window.p75);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, window.p90);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, window.trimmedMean);
    assertQuantileOrdering(window);
}

void test_feature_history_sorted_and_reverse_sorted_quantiles_match() {
    detection::FeatureHistory sortedHistory;
    detection::FeatureHistory reverseHistory;
    recordSeries(sortedHistory, {1.0f, 2.0f, 3.0f, 4.0f});
    recordSeries(reverseHistory, {4.0f, 3.0f, 2.0f, 1.0f});

    const auto sortedWindow = sortedHistory.getWindow(detection::FeatureStreamId::AmpMagnitude, 0, 3, 3);
    const auto reverseWindow = reverseHistory.getWindow(detection::FeatureStreamId::AmpMagnitude, 0, 3, 3);

    TEST_ASSERT_FLOAT_WITHIN(kQuantileEpsilon, 2.5f, sortedWindow.median);
    TEST_ASSERT_FLOAT_WITHIN(kQuantileEpsilon, 3.25f, sortedWindow.p75);
    TEST_ASSERT_FLOAT_WITHIN(kQuantileEpsilon, 3.7f, sortedWindow.p90);
    TEST_ASSERT_FLOAT_WITHIN(kQuantileEpsilon, 2.5f, sortedWindow.trimmedMean);
    TEST_ASSERT_FLOAT_WITHIN(kQuantileEpsilon, sortedWindow.median, reverseWindow.median);
    TEST_ASSERT_FLOAT_WITHIN(kQuantileEpsilon, sortedWindow.p75, reverseWindow.p75);
    TEST_ASSERT_FLOAT_WITHIN(kQuantileEpsilon, sortedWindow.p90, reverseWindow.p90);
    TEST_ASSERT_FLOAT_WITHIN(kQuantileEpsilon, sortedWindow.trimmedMean, reverseWindow.trimmedMean);
    assertQuantileOrdering(sortedWindow);
    assertQuantileOrdering(reverseWindow);
}

void test_feature_history_duplicate_values_quantiles() {
    detection::FeatureHistory history;
    recordSeries(history, {2.0f, 2.0f, 2.0f, 2.0f});
    const auto window = history.getWindow(detection::FeatureStreamId::AmpMagnitude, 0, 3, 3);

    TEST_ASSERT_FLOAT_WITHIN(kQuantileEpsilon, 2.0f, window.median);
    TEST_ASSERT_FLOAT_WITHIN(kQuantileEpsilon, 2.0f, window.p75);
    TEST_ASSERT_FLOAT_WITHIN(kQuantileEpsilon, 2.0f, window.p90);
    TEST_ASSERT_FLOAT_WITHIN(kQuantileEpsilon, 2.0f, window.trimmedMean);
    assertQuantileOrdering(window);
}

void test_next_occurrence_ids_are_unique() {
    const unsigned long first = detection::analyzer::nextOccurrenceId(0);
    const unsigned long second = detection::analyzer::nextOccurrenceId(first);
    TEST_ASSERT_EQUAL_UINT32(1, first);
    TEST_ASSERT_EQUAL_UINT32(2, second);
    TEST_ASSERT_TRUE_MESSAGE(first != second, "accepted occurrences should receive different IDs");
}

void test_scalar_detector_emits_distinct_occurrence_ids() {
    ScalarTransientDetector detector;
    detector.begin();
    detector.setCooldownAfterOnsetMs(0);
    detector.setReleaseDebounceMs(1);
    detector.setMinTransientDurationMs(0);
    detector.setMaxTransientDurationMs(1000);
    detector.setRequireCarrierQuality(false);
    detector.setRequireMinStrength(false);

    detection::Occurrence firstOccurrence = {};
    detection::Occurrence secondOccurrence = {};
    detection::DetectorReport firstReport = {};
    detection::DetectorReport secondReport = {};

    driveAcceptedOccurrence(detector, 0, 6, 25, firstOccurrence, firstReport);
    driveAcceptedOccurrence(detector, 100, 6, 25, secondOccurrence, secondReport);

    TEST_ASSERT_TRUE_MESSAGE(firstOccurrence.present, "first occurrence should be present");
    TEST_ASSERT_TRUE_MESSAGE(secondOccurrence.present, "second occurrence should be present");
    TEST_ASSERT_TRUE_MESSAGE(firstOccurrence.occurrenceId != secondOccurrence.occurrenceId,
                             "accepted occurrences should not reuse occurrence IDs");
    TEST_ASSERT_EQUAL_UINT32(firstOccurrence.occurrenceId, firstReport.accepted.occurrenceId);
    TEST_ASSERT_EQUAL_UINT32(secondOccurrence.occurrenceId, secondReport.accepted.occurrenceId);
    TEST_ASSERT_TRUE_MESSAGE(
        detection::analyzer::sourceReportMatchesIdentity(
            firstReport.accepted.occurrenceId,
            firstOccurrence.occurrenceId,
            firstReport.accepted.startMs,
            firstOccurrence.startMs,
            firstReport.accepted.endMs,
            firstOccurrence.endMs),
        "accepted report should match the occurrence identity");
}

void setup() {
    delay(100);
    UNITY_BEGIN();
    RUN_TEST(test_feature_history_empty_window);
    RUN_TEST(test_feature_history_single_value_quantiles);
    RUN_TEST(test_feature_history_sorted_and_reverse_sorted_quantiles_match);
    RUN_TEST(test_feature_history_duplicate_values_quantiles);
    RUN_TEST(test_next_occurrence_ids_are_unique);
    RUN_TEST(test_scalar_detector_emits_distinct_occurrence_ids);
    UNITY_END();
}

void loop() {}
