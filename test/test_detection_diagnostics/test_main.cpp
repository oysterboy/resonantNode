/*
Step 11e item 7 follow-up: DetectionDiagnostics describes what DetectionRuntime decided.

Item 7 moved the Analyzer's recording (per-frame counters, the 4-slot pipeline event queue, the
detector report capture, the history-incomplete snapshot) out of DetectionRuntime into a side
channel. The Analyzer's SEQ_SOURCE / SEQ_INSPECT / SEQ_EXPLAIN lines and its trial classifier
read that channel, not the verdict queue the Node reads, so a hook called at the wrong point, a
report captured too late or a reset that clears one side and not the other would make the
Analyzer describe a different trial than the detector decided (owner's concern,
docs/refactors/review-fixes-2026-10-25.md, "Handoff 2026-10-26"). The replay and every other
host suite build the Null variant; this one is built with -D ANALYZER_MODE (env native-test-diag)
and pins down:

  1. one event per verdict over every fixture, in the verdict queue's order, carrying the
     verdict's occurrence id and start and a report that names the same occurrence;
  2. the report an event carries is the one captured when its occurrence was popped, also when
     the occurrence waited in the pending inspection queue while a second one refreshed the
     detector's report (the case ReportSlot exists for);
  3. a full event queue loses the event, not the verdict, and the loss is countable;
  4. which reset clears what;
  5. the counters against the frames fed;
  6. the Null variant is empty, and sizeof(DetectionRuntime) for this (Analyzer) variant; the
     Node variant's size is printed by test_replay_corpus (env native-test, the Null build).

Where the handoff's hypotheses differ from the code, the test pins what the code guarantees and
says so at the test (overflow integrity, the history-incomplete snapshot on resetDetectionState).
Run with `pio test -e native-test-diag` (needs a host C++ compiler on PATH).
*/
#include <Arduino.h>
#include <unity.h>

#include <stdio.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "../../src/app/RuntimeDefaults.h"
#include "../../src/app/TimeBase.h"
#include "../../src/audio/AudioSignal.h"
#include "../../src/audio/AudioSource.h"
#include "../../src/detection/DetectionDiagnostics.h"
#include "../../src/detection/DetectionProfile.h"
#include "../../src/detection/DetectionRuntime.h"
#include "../../src/detection/evaluation/OccurrenceVerdict.h"
#include "../../src/detection/features/FreqBandStream.h"
#include "../../src/detection/features/FrequencyMeasurementPacketBuilder.h"
#include "../../src/host/Replay.h"

#ifndef ANALYZER_MODE
#error "test_detection_diagnostics needs -D ANALYZER_MODE (env native-test-diag)"
#endif

// Item 7 of the handoff list: the Node's variant carries nothing, and this build holds the full one.
static_assert(std::is_empty<detection::NullDetectionDiagnostics>::value, "NullDetectionDiagnostics must be empty");
static_assert(sizeof(detection::NullDetectionDiagnostics) == 1, "NullDetectionDiagnostics must be empty");
static_assert(std::is_empty<detection::NullDetectionDiagnostics::ReportSlot>::value, "the Null ReportSlot must be empty");
static_assert(std::is_same<detection::ActiveDetectionDiagnostics, detection::DetectionDiagnostics>::value,
              "ANALYZER_MODE must select the full DetectionDiagnostics");

namespace {

using detection::DetectionDiagnostics;
using detection::DetectionEventKind;
using detection::DetectionPipelineEvent;
using detection::DetectionRuntime;
using detection::OccurrenceVerdict;

// --- the fixtures, through the chain hostreplay::replay() runs -------------

class VectorSource : public AudioSource {
public:
    VectorSource(const std::vector<int32_t>& pcm, uint32_t rate) : _pcm(pcm), _rate(rate) {}
    void begin() override {}
    bool available() override { return _index < _pcm.size(); }
    bool readSample(int& sample, uint64_t& sampleTimeUs64) override {
        if (_index >= _pcm.size()) {
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
    const std::vector<int32_t>& _pcm;
    uint32_t _rate;
    size_t _index = 0;
};

// ~64 KB with the diagnostics: static, as in test_shipped_profile.
DetectionRuntime gRuntime;

struct FixtureRun {
    std::string name;
    size_t framesFed = 0;
    size_t validFrames = 0;
    std::vector<OccurrenceVerdict> verdicts;  // popped from the verdict queue (the Node's path), in order
    std::vector<DetectionPipelineEvent> events;  // popped from the event queue (the Analyzer's path), in order
    std::vector<OccurrenceVerdict> replayVerdicts;  // hostreplay::replay() on the same capture and options
    uint32_t observeFrameCount = 0;
    uint32_t freshDetectorInputCount = 0;
    uint32_t noFreshFrequencySkipCount = 0;
    uint32_t detectorDrainCount = 0;
    uint32_t verdictProducedCount = 0;
    uint32_t verdictEventPushedCount = 0;
    uint32_t verdictEventDroppedCount = 0;
    unsigned long pipelineEventOverflowCount = 0;
    unsigned long detectorReportMismatchCount = 0;
    unsigned long verdictQueueOverflowCount = 0;
};

// hostreplay::replay() for one capture, with both queues drained after every frame (as the
// Analyzer's loop drains the event queue). Every fixture is mode=both or .wav: no preprocessor.
FixtureRun runFixture(const std::string& path, const hostreplay::Options& opt) {
    FixtureRun run;
    hostreplay::Capture cap;
    TEST_ASSERT_TRUE_MESSAGE(hostreplay::loadCapture(path, cap), path.c_str());
    TEST_ASSERT_FALSE_MESSAGE(cap.rawPcm(), "fixture needs the I2S preprocessor; extend runFixture");
    run.name = cap.name;

    VectorSource source(cap.pcm, cap.sampleRateHz);
    std::unique_ptr<AudioSignal> audioSignal(new AudioSignal(source));
    std::unique_ptr<FreqBandStream> band(new FreqBandStream());
    timebase::host::setNowUs64(0);
    audioSignal->begin(false);
    audioSignal->setSmoothingFactor(0.5f);
    audioSignal->setBaselineUpdateFactor(0.005f);
    audioSignal->setBaselineTrackingQuietThreshold(20);
    if (cap.hasBoardBaseline) {
        audioSignal->seedBaseline(cap.boardBaseline);
    } else {
        audioSignal->rebase();
    }
    band->resetState();
    band->setSampleRateHz(cap.sampleRateHz);
    band->setTargetFrequencyHz(opt.targetHz);
    band->setBand(opt.bandLow, opt.bandHigh);
    if (cap.hasBandLead && cap.bandUpdateEvery > 0) {
        for (unsigned long k = 0; k < cap.bandLead % cap.bandUpdateEvery; ++k) {
            band->observeCenteredSample(0, 0);
        }
    }

    const detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    gRuntime.resetState();
    gRuntime.setDetectorSelection(profile.detectorSelection);
    gRuntime.setScalarTransientConfig(profile.scalarTransient);
    gRuntime.setInspectionPlan(profile.inspectionPlan);
    gRuntime.setFieldStateConfig(profile.fieldStateConfig);
    gRuntime.setVerdictQueueEnabled(true);

    const size_t total = cap.pcm.size() + static_cast<size_t>((static_cast<uint64_t>(opt.tailMs) * cap.sampleRateHz) / 1000ULL);
    for (size_t i = 0; i < total; ++i) {
        const int sample = i < cap.pcm.size() ? cap.pcm[i] : 0;
        const uint64_t us = source.timeUs(i);
        timebase::host::setNowUs64(us);
        AudioSamplePacket packet;
        audioSignal->update(sample, us, packet, false);
        band->observeCenteredSample(packet.baselineCorrectedValue, packet.timeMs);
        const detection::FrequencyBandMeasurementPacket evidence = detection::buildFrequencyMeasurementPacket(*band, packet);
        gRuntime.observeFrame(packet, evidence, packet.timeMs);
        ++run.framesFed;
        if (packet.valid) {
            ++run.validFrames;
        }
        OccurrenceVerdict verdict;
        while (gRuntime.popOccurrenceVerdict(verdict)) {
            run.verdicts.push_back(verdict);
        }
        DetectionPipelineEvent event;
        while (gRuntime.diagnostics().popPipelineEvent(event)) {
            run.events.push_back(event);
        }
    }

    const DetectionDiagnostics& d = gRuntime.diagnostics();
    run.observeFrameCount = d.observeFrameCount();
    run.freshDetectorInputCount = d.freshDetectorInputCount();
    run.noFreshFrequencySkipCount = d.noFreshFrequencySkipCount();
    run.detectorDrainCount = d.detectorDrainCount();
    run.verdictProducedCount = d.verdictProducedCount();
    run.verdictEventPushedCount = d.verdictEventPushedCount();
    run.verdictEventDroppedCount = d.verdictEventDroppedCount();
    run.pipelineEventOverflowCount = d.pipelineEventOverflowCount();
    run.detectorReportMismatchCount = d.detectorReportMismatchCount();
    run.verdictQueueOverflowCount = gRuntime.verdictQueueOverflowCount();
    run.replayVerdicts = hostreplay::replay(cap, opt, profile).verdicts;
    return run;
}

hostreplay::Options networkBand() {
    hostreplay::Options opt;
    opt.bandLow = runtime::kDefaultNetworkBandLowHz;
    opt.bandHigh = runtime::kDefaultNetworkBandHighHz;
    return opt;
}

hostreplay::Options singleTarget3200() {
    hostreplay::Options opt;
    opt.bandLow = 0;
    opt.bandHigh = 0;
    opt.targetHz = 3200;
    return opt;
}

std::vector<std::string> fixturePaths() {
    std::vector<std::string> paths;
    for (const auto& entry : std::filesystem::directory_iterator("test/fixtures")) {
        const std::string ext = entry.path().extension().string();
        if (ext == ".csv" || ext == ".wav") {
            paths.push_back(entry.path().generic_string());
        }
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

// Every fixture under both band settings, run once and shared by the fixture tests.
const std::vector<FixtureRun>& fixtureRuns() {
    static std::vector<FixtureRun> runs;
    static bool done = false;
    if (!done) {
        done = true;
        const hostreplay::Options options[] = {networkBand(), singleTarget3200()};
        for (const std::string& path : fixturePaths()) {
            for (const hostreplay::Options& opt : options) {
                runs.push_back(runFixture(path, opt));
            }
        }
    }
    return runs;
}

// --- synthetic streams straight into observeFrame --------------------------

// The shipped detector thresholds on the amp stream (no fresh-frequency gate), one inspector
// on the amp stream with a configurable window: frames are 1 ms apart and carry the value
// the detector and the history both read.
detection::DetectionProfile syntheticProfile(unsigned long windowPreMs, unsigned long windowPostMs) {
    detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    profile.scalarTransient.observedStream = detection::FeatureStreamId::AmpMagnitude;
    profile.inspectionPlan.modules[0] = profile.inspectionPlan.modules[1];  // the amp inspector
    profile.inspectionPlan.modules[0].magnitude.windowPreMs = windowPreMs;
    profile.inspectionPlan.modules[0].magnitude.windowPostMs = windowPostMs;
    profile.inspectionPlan.modules[1] = detection::InspectionModuleConfig{};
    profile.inspectionPlan.count = 1;
    return profile;
}

void configureSynthetic(const detection::DetectionProfile& profile) {
    gRuntime.resetState();
    gRuntime.setDetectorSelection(profile.detectorSelection);
    gRuntime.setScalarTransientConfig(profile.scalarTransient);
    gRuntime.setInspectionPlan(profile.inspectionPlan);
    gRuntime.setFieldStateConfig(profile.fieldStateConfig);
    gRuntime.setVerdictQueueEnabled(true);
}

struct SyntheticFeed {
    unsigned long ms = 1000;
    size_t frames = 0;
    size_t validFrames = 0;
    bool drainVerdicts = true;
    bool drainEvents = false;
    std::vector<OccurrenceVerdict> verdicts;
    std::vector<DetectionPipelineEvent> events;

    void frame(float value, bool valid = true) {
        AudioSamplePacket packet = {};
        packet.timeUs64 = static_cast<uint64_t>(ms) * 1000ULL;
        packet.timeMs = static_cast<uint32_t>(ms);
        packet.sampleRateHz = 1000;
        packet.valid = valid;
        packet.audioMagnitudeValue = static_cast<audio::Strength16>(value);
        packet.smoothedLevel = static_cast<audio::Strength16>(value);
        packet.level = static_cast<int>(value);
        const detection::FrequencyBandMeasurementPacket evidence = {};
        timebase::host::setNowUs64(packet.timeUs64);
        gRuntime.observeFrame(packet, evidence, ms);
        ++ms;
        ++frames;
        if (valid) {
            ++validFrames;
        }
        OccurrenceVerdict verdict;
        while (drainVerdicts && gRuntime.popOccurrenceVerdict(verdict)) {
            verdicts.push_back(verdict);
        }
        DetectionPipelineEvent event;
        while (drainEvents && gRuntime.diagnostics().popPipelineEvent(event)) {
            events.push_back(event);
        }
    }
    void hold(unsigned long count, float value) {
        for (unsigned long i = 0; i < count; ++i) {
            frame(value);
        }
    }
    // A 120 ms pulse at 5000 (above every shipped scalar gate) and `quietMs` of silence.
    void pulse(unsigned long quietMs) {
        hold(120, 5000.0f);
        hold(quietMs, 0.0f);
    }
    void drainEventsNow() {
        DetectionPipelineEvent event;
        while (gRuntime.diagnostics().popPipelineEvent(event)) {
            events.push_back(event);
        }
    }
};

size_t acceptedEventCount(const std::vector<DetectionPipelineEvent>& events) {
    size_t n = 0;
    for (const DetectionPipelineEvent& e : events) {
        if (e.kind == DetectionEventKind::AcceptedPipelineResult) {
            ++n;
        }
    }
    return n;
}

}  // namespace

void setUp() {}
void tearDown() {}

// The fixture loop above is hostreplay::replay(): same verdicts, so what follows is about the
// chain the replay (T2h) and test_replay_corpus check, with the full diagnostics compiled in.
void test_fixture_loop_is_the_replay() {
    const std::vector<FixtureRun>& runs = fixtureRuns();
    TEST_ASSERT_TRUE(runs.size() >= 2 * 14);
    size_t verdictTotal = 0;
    for (const FixtureRun& run : runs) {
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(run.replayVerdicts.size(), run.verdicts.size(), run.name.c_str());
        for (size_t i = 0; i < run.verdicts.size(); ++i) {
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(run.replayVerdicts[i].occurrenceId, run.verdicts[i].occurrenceId, run.name.c_str());
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(run.replayVerdicts[i].primaryStartMs, run.verdicts[i].primaryStartMs, run.name.c_str());
            TEST_ASSERT_EQUAL_MESSAGE(run.replayVerdicts[i].accepted, run.verdicts[i].accepted, run.name.c_str());
        }
        verdictTotal += run.verdicts.size();
    }
    TEST_ASSERT_TRUE_MESSAGE(verdictTotal >= 10, "the fixtures should produce verdicts to compare");
}

// Handoff item 2: one event per verdict. Every verdict the runtime queued has exactly one
// AcceptedPipelineResult event (the kind covers rejected verdicts too), in the same order,
// with the verdict's occurrence id and start; the report the event carries names the same
// occurrence. Nothing is dropped or mismatched on the fixtures.
void test_one_event_per_verdict_over_every_fixture() {
    for (const FixtureRun& run : fixtureRuns()) {
        const char* name = run.name.c_str();
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, run.verdictQueueOverflowCount, name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(run.verdicts.size(), run.verdictProducedCount, name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(run.verdictProducedCount, run.verdictEventPushedCount + run.verdictEventDroppedCount, name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(run.verdictEventPushedCount, acceptedEventCount(run.events), name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(run.verdictProducedCount, acceptedEventCount(run.events) + run.verdictEventDroppedCount, name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, run.verdictEventDroppedCount, name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, run.pipelineEventOverflowCount, name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, run.detectorReportMismatchCount, name);

        size_t v = 0;
        uint32_t lastEventId = 0;
        for (const DetectionPipelineEvent& e : run.events) {
            TEST_ASSERT_TRUE_MESSAGE(e.eventId > lastEventId, name);  // event ids ascend in queue order
            lastEventId = e.eventId;
            if (e.kind != DetectionEventKind::AcceptedPipelineResult) {
                TEST_ASSERT_FALSE_MESSAGE(e.hasOccurrenceId, name);
                TEST_ASSERT_TRUE_MESSAGE(e.hasCandidateId && e.candidateId != 0, name);
                continue;
            }
            TEST_ASSERT_TRUE_MESSAGE(v < run.verdicts.size(), name);
            const OccurrenceVerdict& verdict = run.verdicts[v++];
            TEST_ASSERT_TRUE_MESSAGE(e.hasOccurrenceId && e.hasVerdict, name);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(verdict.occurrenceId, e.occurrenceId, name);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(verdict.occurrenceId, e.verdict.occurrenceId, name);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(verdict.primaryStartMs, e.verdict.primaryStartMs, name);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(verdict.primaryDurationMs, e.verdict.primaryDurationMs, name);
            TEST_ASSERT_EQUAL_MESSAGE(verdict.accepted, e.verdict.accepted, name);
            TEST_ASSERT_TRUE_MESSAGE(e.detectorReportPresent, name);
            TEST_ASSERT_TRUE_MESSAGE(e.detectorReportMatched, name);
            TEST_ASSERT_TRUE_MESSAGE(e.integrity.correlationComplete, name);
            TEST_ASSERT_EQUAL_MESSAGE(detection::PipelineIntegrityReason::None, e.integrity.reason, name);
            TEST_ASSERT_TRUE_MESSAGE(e.sourceRecord.sourceReportMatched, name);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(e.eventId, e.sourceRecord.eventId, name);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(verdict.occurrenceId, e.sourceRecord.detectorReport.accepted.occurrenceId, name);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(verdict.primaryStartMs, e.sourceRecord.detectorReport.accepted.startMs, name);
            TEST_ASSERT_TRUE_MESSAGE(e.hasInspectedOccurrence, name);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(verdict.occurrenceId, e.inspectedOccurrence.occurrence.occurrenceId, name);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(verdict.occurrenceId, e.sourceOccurrenceId, name);
            TEST_ASSERT_EQUAL_STRING_MESSAGE(
                e.inspectedOccurrence.decision == detection::OccurrenceDecision::Rejected ? "selected_reject" : "selected_occurrence",
                e.sourceSelection, name);
        }
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(run.verdicts.size(), v, name);
    }
}

// Handoff item 6: the counters against the frames fed. Every frame is counted, every valid
// frame either reaches the detector or is skipped for want of a fresh frequency packet, and a
// drain runs at most once per valid frame (and on every frame the detector saw).
void test_counters_match_the_frames_fed_over_every_fixture() {
    for (const FixtureRun& run : fixtureRuns()) {
        const char* name = run.name.c_str();
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(run.framesFed, run.observeFrameCount, name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(run.validFrames, run.freshDetectorInputCount + run.noFreshFrequencySkipCount, name);
        TEST_ASSERT_TRUE_MESSAGE(run.noFreshFrequencySkipCount > 0, name);  // the shipped profile reads FrequencyTarget
        TEST_ASSERT_TRUE_MESSAGE(run.detectorDrainCount >= run.freshDetectorInputCount, name);
        TEST_ASSERT_TRUE_MESSAGE(run.detectorDrainCount <= run.validFrames, name);
        TEST_ASSERT_TRUE_MESSAGE(run.detectorDrainCount <= run.framesFed, name);
    }
}

// The same on a synthetic stream with invalid frames in it (they count as observed only), on
// the amp stream (no fresh-frequency skip), with one occurrence produced and popped.
void test_counters_match_the_frames_fed_with_invalid_frames() {
    configureSynthetic(syntheticProfile(0, 100));
    SyntheticFeed feed;
    feed.drainEvents = true;
    feed.hold(100, 0.0f);
    for (int i = 0; i < 25; ++i) {
        feed.frame(0.0f, false);
    }
    feed.pulse(300);
    const DetectionDiagnostics& d = gRuntime.diagnostics();
    TEST_ASSERT_EQUAL_UINT32(feed.frames, d.observeFrameCount());
    TEST_ASSERT_EQUAL_UINT32(feed.validFrames, d.freshDetectorInputCount() + d.noFreshFrequencySkipCount());
    TEST_ASSERT_EQUAL_UINT32(0, d.noFreshFrequencySkipCount());
    TEST_ASSERT_EQUAL_UINT32(feed.validFrames, d.detectorDrainCount());  // the amp stream drains every valid frame
    TEST_ASSERT_EQUAL_UINT32(1, d.detectorOccurrencePoppedCount());
    TEST_ASSERT_EQUAL_UINT32(1, d.detectorValidOccurrencePoppedCount());
    TEST_ASSERT_EQUAL_UINT32(1, d.verdictProducedCount());
    TEST_ASSERT_EQUAL_UINT32(1, feed.verdicts.size());
    TEST_ASSERT_EQUAL_UINT32(1, acceptedEventCount(feed.events));
}

// Handoff item 3: the report captured at pop time. The inspection window is stretched to
// 400 ms so the first occurrence waits in the pending queue while a second one closes and
// refreshes the detector's report; the first event must carry the first occurrence's report
// (ReportSlot), while the detector's report has already moved on to the second.
void test_report_is_captured_when_the_occurrence_is_popped() {
    configureSynthetic(syntheticProfile(0, 400));
    SyntheticFeed feed;
    feed.hold(100, 0.0f);
    feed.pulse(30);   // occurrence 1 closes; its window ends 400 ms after its start
    feed.pulse(30);   // occurrence 2 closes while occurrence 1 still waits
    const DetectionDiagnostics& d = gRuntime.diagnostics();
    TEST_ASSERT_EQUAL_UINT32(2, d.detectorOccurrencePoppedCount());
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, d.verdictProducedCount(), "occurrence 1 should still be waiting");

    // Run until occurrence 1's verdict; stop on that frame.
    DetectionPipelineEvent first;
    bool haveFirst = false;
    for (int i = 0; i < 600 && !haveFirst; ++i) {
        feed.frame(0.0f);
        haveFirst = gRuntime.diagnostics().popPipelineEvent(first);
    }
    TEST_ASSERT_TRUE(haveFirst);
    TEST_ASSERT_EQUAL(DetectionEventKind::AcceptedPipelineResult, first.kind);
    TEST_ASSERT_EQUAL_UINT32(1, feed.verdicts.size());
    const unsigned long firstId = feed.verdicts[0].occurrenceId;
    TEST_ASSERT_EQUAL_UINT32(firstId, first.occurrenceId);
    // The detector's latest report names occurrence 2 at this point (afterDrain has captured it;
    // reading it here changes nothing the frame still has to do).
    const detection::DetectorReport& live = gRuntime.diagnostics().activeDetectorReport();
    TEST_ASSERT_TRUE(live.accepted.present);
    TEST_ASSERT_TRUE_MESSAGE(live.accepted.occurrenceId != firstId, "the second occurrence should have refreshed the report");
    // The event still carries the report occurrence 1 was popped with.
    TEST_ASSERT_TRUE(first.detectorReportMatched);
    TEST_ASSERT_TRUE(first.integrity.correlationComplete);
    TEST_ASSERT_EQUAL_UINT32(firstId, first.sourceRecord.detectorReport.accepted.occurrenceId);
    TEST_ASSERT_EQUAL_UINT32(feed.verdicts[0].primaryStartMs, first.sourceRecord.detectorReport.accepted.startMs);
    TEST_ASSERT_EQUAL_UINT32(0, d.detectorReportMismatchCount());

    // Occurrence 2 follows with its own report.
    feed.drainEvents = true;
    feed.hold(600, 0.0f);
    TEST_ASSERT_EQUAL_UINT32(2, feed.verdicts.size());
    TEST_ASSERT_EQUAL_UINT32(1, acceptedEventCount(feed.events));
    const DetectionPipelineEvent& second = feed.events[0];
    TEST_ASSERT_EQUAL_UINT32(feed.verdicts[1].occurrenceId, second.occurrenceId);
    TEST_ASSERT_EQUAL_UINT32(live.accepted.occurrenceId, second.occurrenceId);
    TEST_ASSERT_TRUE(second.detectorReportMatched);
    TEST_ASSERT_EQUAL_UINT32(second.occurrenceId, second.sourceRecord.detectorReport.accepted.occurrenceId);
    TEST_ASSERT_EQUAL_UINT32(0, d.detectorReportMismatchCount());
    TEST_ASSERT_EQUAL_UINT32(0, gRuntime.pendingInspectionOverflowCount());
}

// Handoff item 4: five verdicts, the verdict queue drained (the Node's path), the 4-slot event
// queue not. The fifth event is lost and counted (overflow 1, dropped 1), the verdict queue
// loses nothing, and the next event's id shows the gap.
// Differs from the handoff's hypothesis: the runtime's events never set
// integrity.queueOverflowAffected / PipelineEventQueueOverflow (DetectionDiagnostics::onVerdict
// sets reason None / MissingVerdict / MissingDetectorReport only). The Analyzer derives the
// trial's queue_overflow from the pipelineEventOverflowCount delta over the trial
// (AnalyzerModeApp.cpp), so the count is what this pins down.
void test_event_queue_overflow_loses_the_event_not_the_verdict() {
    configureSynthetic(syntheticProfile(0, 100));
    SyntheticFeed feed;
    feed.hold(100, 0.0f);
    for (int i = 0; i < 5; ++i) {
        feed.pulse(200);
    }
    const DetectionDiagnostics& d = gRuntime.diagnostics();
    TEST_ASSERT_EQUAL_UINT32(5, feed.verdicts.size());
    TEST_ASSERT_EQUAL_UINT32(0, gRuntime.verdictQueueOverflowCount());
    TEST_ASSERT_EQUAL_UINT32(5, d.verdictProducedCount());
    TEST_ASSERT_EQUAL_UINT32(4, d.verdictEventPushedCount());
    TEST_ASSERT_EQUAL_UINT32(1, d.verdictEventDroppedCount());
    TEST_ASSERT_EQUAL_UINT32(1, d.pipelineEventOverflowCount());

    feed.drainEventsNow();
    TEST_ASSERT_EQUAL_UINT32(4, feed.events.size());
    for (size_t i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL(DetectionEventKind::AcceptedPipelineResult, feed.events[i].kind);
        TEST_ASSERT_EQUAL_UINT32(feed.verdicts[i].occurrenceId, feed.events[i].occurrenceId);
        TEST_ASSERT_EQUAL_UINT32(i + 1, feed.events[i].eventId);
        TEST_ASSERT_TRUE(feed.events[i].detectorReportMatched);
    }

    // The next verdict's event: in the queue again, id 6 (5 went to the lost event).
    feed.drainEvents = true;
    feed.pulse(200);
    TEST_ASSERT_EQUAL_UINT32(6, feed.verdicts.size());
    TEST_ASSERT_EQUAL_UINT32(5, feed.events.size());
    const DetectionPipelineEvent& next = feed.events[4];
    TEST_ASSERT_EQUAL_UINT32(feed.verdicts[5].occurrenceId, next.occurrenceId);
    TEST_ASSERT_EQUAL_UINT32(6, next.eventId);
    TEST_ASSERT_TRUE(next.detectorReportMatched);
    TEST_ASSERT_FALSE(next.integrity.queueOverflowAffected);  // see above: not set by the runtime
    TEST_ASSERT_EQUAL(detection::PipelineIntegrityReason::None, next.integrity.reason);
    TEST_ASSERT_EQUAL_UINT32(1, d.pipelineEventOverflowCount());
    TEST_ASSERT_EQUAL_UINT32(0, gRuntime.verdictQueueOverflowCount());
}

// Handoff item 5a: resetDiagnosticsCounters (the Analyzer's per-trial reset with diagnostics
// on) zeroes every counter and the history-incomplete snapshot, and leaves queued events.
void test_reset_diagnostics_counters_keeps_the_event_queue() {
    // A 100 ms look-back with 30 ms of history before the onset: the inspection finds its
    // window incomplete, which fills the snapshot.
    configureSynthetic(syntheticProfile(100, 100));
    SyntheticFeed feed;
    feed.hold(30, 0.0f);
    feed.pulse(200);
    feed.pulse(200);
    DetectionDiagnostics& d = gRuntime.diagnostics();
    TEST_ASSERT_EQUAL_UINT32(2, d.verdictProducedCount());
    TEST_ASSERT_EQUAL_UINT32(1, d.historyIncompleteSnapshot().count);  // only the first lacked its look-back
    TEST_ASSERT_TRUE(d.observeFrameCount() > 0);

    gRuntime.resetDiagnosticsCounters();
    TEST_ASSERT_EQUAL_UINT32(0, d.observeFrameCount());
    TEST_ASSERT_EQUAL_UINT32(0, d.freshDetectorInputCount());
    TEST_ASSERT_EQUAL_UINT32(0, d.noFreshFrequencySkipCount());
    TEST_ASSERT_EQUAL_UINT32(0, d.detectorDrainCount());
    TEST_ASSERT_EQUAL_UINT32(0, d.detectorReportRefreshCount());
    TEST_ASSERT_EQUAL_UINT32(0, d.detectorOccurrencePoppedCount());
    TEST_ASSERT_EQUAL_UINT32(0, d.detectorValidOccurrencePoppedCount());
    TEST_ASSERT_EQUAL_UINT32(0, d.verdictProducedCount());
    TEST_ASSERT_EQUAL_UINT32(0, d.verdictEventPushedCount());
    TEST_ASSERT_EQUAL_UINT32(0, d.verdictEventDroppedCount());
    TEST_ASSERT_EQUAL_UINT32(0, d.pipelineEventOverflowCount());
    TEST_ASSERT_EQUAL_UINT32(0, d.detectorReportMismatchCount());
    TEST_ASSERT_EQUAL_UINT32(0, d.historyIncompleteSnapshot().count);
    TEST_ASSERT_EQUAL_UINT32(0, gRuntime.verdictQueueOverflowCount());

    feed.drainEventsNow();
    TEST_ASSERT_EQUAL_UINT32(2, acceptedEventCount(feed.events));
    TEST_ASSERT_EQUAL_UINT32(feed.verdicts[0].occurrenceId, feed.events[0].occurrenceId);
    TEST_ASSERT_EQUAL_UINT32(feed.verdicts[1].occurrenceId, feed.events[1].occurrenceId);
}

// Handoff item 5b: resetDetectionState (a SEQ trial boundary without the counter reset) leaves
// no event of the previous trial behind, restarts the event ids, and an occurrence that was
// waiting for its window at the reset produces neither a verdict nor an event afterwards.
// Differs from the handoff's hypothesis: the history-incomplete snapshot is not cleared with
// the history. It is a counter-like record (SEQ_HISTDBG prints when its count grew since the
// previous trial line, AnalyzerSeqReporter.cpp) and is cleared by resetDiagnosticsCounters and
// resetState, which SEQ start calls (AnalyzerSequenceSession.cpp); this pins that split.
// The events' sourceRecord.eventTrialAttribution is always 0 from the runtime: the Analyzer
// stamps the trial when it captures the event (AnalyzerTrialCapture.cpp), so trial attribution
// of a stale event is excluded here by the queue being empty, not by that field.
void test_reset_detection_state_leaves_no_event_behind() {
    configureSynthetic(syntheticProfile(100, 400));
    SyntheticFeed feed;
    feed.hold(30, 0.0f);
    feed.pulse(500);  // verdict 1 (window incomplete on the left: the snapshot fills)
    feed.pulse(500);  // verdict 2
    feed.pulse(30);   // occurrence 3 popped, waiting for its 400 ms window
    DetectionDiagnostics& d = gRuntime.diagnostics();
    TEST_ASSERT_EQUAL_UINT32(2, d.verdictProducedCount());
    TEST_ASSERT_EQUAL_UINT32(3, d.detectorOccurrencePoppedCount());
    TEST_ASSERT_EQUAL_UINT32(1, d.historyIncompleteSnapshot().count);
    const uint32_t framesBefore = d.observeFrameCount();

    gRuntime.resetDetectionState();
    DetectionPipelineEvent stale;
    TEST_ASSERT_FALSE(d.popPipelineEvent(stale));
    TEST_ASSERT_EQUAL_UINT32(0, d.pipelineEventOverflowCount());
    // Not cleared by this reset (see above), nor are the counters.
    TEST_ASSERT_EQUAL_UINT32(1, d.historyIncompleteSnapshot().count);
    TEST_ASSERT_EQUAL_UINT32(framesBefore, d.observeFrameCount());
    TEST_ASSERT_EQUAL_UINT32(2, d.verdictProducedCount());

    // Occurrence 3 died with the pending queue: silence brings no verdict and no event.
    feed.drainEvents = true;
    feed.hold(600, 0.0f);
    TEST_ASSERT_EQUAL_UINT32(2, feed.verdicts.size());
    TEST_ASSERT_EQUAL_UINT32(0, feed.events.size());
    TEST_ASSERT_EQUAL_UINT32(2, d.verdictProducedCount());

    // The next trial's first verdict gets event id 1 and its own report.
    feed.pulse(500);
    TEST_ASSERT_EQUAL_UINT32(3, feed.verdicts.size());
    TEST_ASSERT_EQUAL_UINT32(1, acceptedEventCount(feed.events));
    TEST_ASSERT_EQUAL_UINT32(1, feed.events[0].eventId);
    TEST_ASSERT_EQUAL_UINT32(feed.verdicts[2].occurrenceId, feed.events[0].occurrenceId);
    TEST_ASSERT_TRUE(feed.events[0].detectorReportMatched);
    TEST_ASSERT_EQUAL_UINT32(0, feed.events[0].sourceRecord.eventTrialAttribution);

    // resetState (SEQ start: detection state and counters) clears the snapshot.
    gRuntime.resetState();
    TEST_ASSERT_EQUAL_UINT32(0, d.historyIncompleteSnapshot().count);
    TEST_ASSERT_EQUAL_UINT32(0, d.observeFrameCount());
    TEST_ASSERT_FALSE(d.popPipelineEvent(stale));
}

// Handoff item 7: the sizes. The static_asserts at the top hold the Null variant empty; the
// Node variant's sizeof(DetectionRuntime) is printed by test_replay_corpus (Null build). A
// static_assert on the byte count would be too brittle (handoff), so this prints it.
void test_sizes_probe() {
    char line[160];
    snprintf(line, sizeof(line),
             "sizeof(DetectionRuntime) Analyzer variant (ANALYZER_MODE) = %u, DetectionDiagnostics = %u, NullDetectionDiagnostics = %u",
             static_cast<unsigned>(sizeof(DetectionRuntime)), static_cast<unsigned>(sizeof(DetectionDiagnostics)),
             static_cast<unsigned>(sizeof(detection::NullDetectionDiagnostics)));
    printf("%s\n", line);
    TEST_MESSAGE(line);
    TEST_ASSERT_TRUE(sizeof(DetectionRuntime) > sizeof(DetectionDiagnostics));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fixture_loop_is_the_replay);
    RUN_TEST(test_one_event_per_verdict_over_every_fixture);
    RUN_TEST(test_counters_match_the_frames_fed_over_every_fixture);
    RUN_TEST(test_counters_match_the_frames_fed_with_invalid_frames);
    RUN_TEST(test_report_is_captured_when_the_occurrence_is_popped);
    RUN_TEST(test_event_queue_overflow_loses_the_event_not_the_verdict);
    RUN_TEST(test_reset_diagnostics_counters_keeps_the_event_queue);
    RUN_TEST(test_reset_detection_state_leaves_no_event_behind);
    RUN_TEST(test_sizes_probe);
    return UNITY_END();
}
