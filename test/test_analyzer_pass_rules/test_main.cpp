#include <Arduino.h>
#include <unity.h>

#include "../../src/audio/AudioSignal.h"
#include "../../src/detection/analyzer/AnalyzerPassRules.h"
#include "../../src/detection/detectors/scalar/ScalarTransientDetector.h"

// Pull the detector implementation into the test build so the regression can
// run without changing the firmware build configuration.
#include "../../src/detection/detectors/scalar/ScalarTransientDetector.cpp"
#include "../../src/detection/detectors/scalar/ScalarTransientOccurrence.cpp"
#include "../../src/detection/detectors/scalar/ScalarTransientReport.cpp"

namespace {

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
    RUN_TEST(test_next_occurrence_ids_are_unique);
    RUN_TEST(test_scalar_detector_emits_distinct_occurrence_ids);
    UNITY_END();
}

void loop() {}
