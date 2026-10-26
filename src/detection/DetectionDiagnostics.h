#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../app/RingQueue.h"
#include "detectors/DetectorReport.h"
#include "detectors/scalar/ScalarTransientDetector.h"
#include "evaluation/OccurrenceVerdict.h"
#include "features/FeatureHistory.h"
#include "field/FieldState.h"
#include "occurrences/InspectedOccurrence.h"
#include "occurrences/Occurrence.h"

namespace detection {

/*
DetectionDiagnostics (step 11e item 7)

Everything DetectionRuntime records for the Analyzer and nothing the Node
needs: the per-frame counters, the pipeline event queue (one event per
accepted verdict and per new selected reject, SEQ_SOURCE / SEQ_INSPECT /
SEQ_EXPLAIN read them), the detector report capture and the snapshot of the
history ring at the last incomplete inspection window (issue #26).

DetectionRuntime owns one `ActiveDetectionDiagnostics` and calls its hooks
unconditionally; which class that is depends on the build. The Analyzer
build gets `DetectionDiagnostics`; every other build gets
`NullDetectionDiagnostics`, whose hooks are empty inline functions, so the
Node binary carries none of this (cleanup-analyzer-node-isolation.md) and the
runtime's bodies carry no #ifdef. The Analyzer reaches the diagnostics
through DetectionRuntime::diagnostics().
*/

// --- the Analyzer-facing records -------------------------------------------

struct SourceDiagnosticRecord {
    DetectorReport detectorReport = {};
    const char* sourceSelection = "none";
    uint32_t eventId = 0;
    uint32_t reportGeneration = 0;
    unsigned long eventTrialAttribution = 0;
    unsigned long sourceOccurrenceId = 0;
    unsigned long sourceCandidateId = 0;
    bool sourceReportMatched = false;
};

enum class PipelineIntegrityReason {
    None,
    MissingDetectorReport,
    MissingVerdict,
    InspectionQueueOverflow,
    VerdictQueueOverflow,
    PipelineEventQueueOverflow,
};

struct PipelineIntegrity {
    bool detectorReportPresent = false;
    bool occurrenceMatched = false;
    bool inspectionPresent = false;
    bool verdictPresent = false;
    bool correlationComplete = false;
    bool queueOverflowAffected = false;
    PipelineIntegrityReason reason = PipelineIntegrityReason::None;
};

enum class DetectionEventKind {
    AcceptedPipelineResult,
    RejectedSourceCandidate,
};

struct DetectionPipelineEvent {
    DetectionEventKind kind = DetectionEventKind::AcceptedPipelineResult;
    uint32_t eventId = 0;
    bool hasOccurrenceId = false;
    uint32_t occurrenceId = 0;
    bool hasCandidateId = false;
    uint32_t candidateId = 0;
    bool detectorReportPresent = false;
    bool detectorReportMatched = false;
    const char* sourceSelection = "none";
    unsigned long sourceOccurrenceId = 0;
    unsigned long sourceCandidateId = 0;
    PipelineIntegrity integrity = {};

    bool hasVerdict = false;
    OccurrenceVerdict verdict = {};

    bool hasInspectedOccurrence = false;
    InspectedOccurrence inspectedOccurrence = {};

    bool hasSourceRecord = false;
    SourceDiagnosticRecord sourceRecord = {};
};

// Snapshot of the history ring at the latest inspection that found its
// window incomplete (issue #26 diagnostics).
struct HistoryIncompleteSnapshot {
    uint32_t count = 0;
    FeatureStreamId stream = FeatureStreamId::Unknown;
    unsigned long requestedStartMs = 0;
    unsigned long requestedEndMs = 0;
    unsigned long inspectionNowMs = 0;
    unsigned long ringOldestMs = 0;
    unsigned long ringNewestMs = 0;
    size_t ringBins = 0;
    uint32_t outOfOrderRecords = 0;
};

// --- the Node's variant: nothing recorded ----------------------------------

class NullDetectionDiagnostics {
public:
    // What a waiting occurrence keeps for its later event: nothing here.
    struct ReportSlot {};

    NullDetectionDiagnostics(ScalarTransientDetector&, const FeatureHistory&) {}

    void resetCounters() {}
    void resetBookkeeping() {}
    void onObserveFrame() {}
    void onNoFreshFrequencySkip() {}
    void onFreshDetectorInput() {}
    void onDrain() {}
    void onOccurrencePopped(const Occurrence&) {}
    void captureReport(ReportSlot&) const {}
    void onInspected(const InspectedOccurrence&) {}
    void onVerdict(const OccurrenceVerdict&, const InspectedOccurrence&, const ReportSlot&, unsigned long) {}
    void afterDrain() {}
};

// --- the Analyzer's variant --------------------------------------------------

class DetectionDiagnostics {
public:
    struct ReportSlot {
        DetectorReport report = {};
    };
    static constexpr size_t kPipelineEventQueueCapacity = 4;

    DetectionDiagnostics(ScalarTransientDetector& detector, const FeatureHistory& history);

    // Hooks DetectionRuntime calls, in the order of its frame.
    void resetCounters();
    void resetBookkeeping();
    void onObserveFrame() { ++_observeFrameCount; }
    void onNoFreshFrequencySkip() { ++_noFreshFrequencySkipCount; }
    void onFreshDetectorInput() { ++_freshDetectorInputCount; }
    void onDrain() { ++_detectorDrainCount; }
    void onOccurrencePopped(const Occurrence& occurrence);
    // The detector's report at pop time, kept with an occurrence that waits
    // for its inspection window (its event names the report it was popped with).
    void captureReport(ReportSlot& slot) const { slot.report = _detector.latestReport(); }
    void onInspected(const InspectedOccurrence& inspected);
    void onVerdict(const OccurrenceVerdict& verdict, const InspectedOccurrence& inspected, const ReportSlot& slot,
                   unsigned long nowMs);
    // After the drain: one RejectedSourceCandidate event per new selected
    // reject (decision 2026-10-13).
    void afterDrain();

    // The Analyzer's reads.
    bool popPipelineEvent(DetectionPipelineEvent& out) { return _pipelineEvents.pop(out); }
    unsigned long pipelineEventOverflowCount() const { return _pipelineEventOverflowCount; }
    unsigned long detectorReportMismatchCount() const { return _detectorReportMismatchCount; }
    uint32_t observeFrameCount() const { return _observeFrameCount; }
    uint32_t freshDetectorInputCount() const { return _freshDetectorInputCount; }
    uint32_t detectorDrainCount() const { return _detectorDrainCount; }
    uint32_t detectorReportRefreshCount() const { return _detectorReportRefreshCount; }
    uint32_t noFreshFrequencySkipCount() const { return _noFreshFrequencySkipCount; }
    uint32_t detectorOccurrencePoppedCount() const { return _detectorOccurrencePoppedCount; }
    uint32_t detectorValidOccurrencePoppedCount() const { return _detectorValidOccurrencePoppedCount; }
    uint32_t verdictProducedCount() const { return _verdictProducedCount; }
    uint32_t verdictEventPushedCount() const { return _verdictEventPushedCount; }
    uint32_t verdictEventDroppedCount() const { return _verdictEventDroppedCount; }
    // The detector's latest frozen report (captured on read if it changed).
    const DetectorReport& activeDetectorReport();
    const FeatureHistory& featureHistory() const { return _history; }
    const HistoryIncompleteSnapshot& historyIncompleteSnapshot() const { return _historyIncomplete; }

private:
    bool pushPipelineEvent(const DetectionPipelineEvent& event);
    bool captureLatestDetectorReportIfChanged();

    ScalarTransientDetector& _detector;
    const FeatureHistory& _history;

    DetectorReport _detectorReport = {};
    uint32_t _lastObservedScalarReportGeneration = 0;
    unsigned long _pipelineEventOverflowCount = 0;
    unsigned long _detectorReportMismatchCount = 0;
    HistoryIncompleteSnapshot _historyIncomplete = {};
    RingQueue<DetectionPipelineEvent, kPipelineEventQueueCapacity> _pipelineEvents;
    unsigned long _pipelineEventSequenceId = 0;
    uint32_t _lastEmittedAcceptedOccurrenceId = 0;
    uint32_t _lastEmittedAcceptedReportGeneration = 0;
    uint32_t _lastEmittedSelectedRejectOccurrenceId = 0;
    uint32_t _lastEmittedSelectedRejectReportGeneration = 0;
    uint32_t _verdictProducedCount = 0;
    uint32_t _verdictEventPushedCount = 0;
    uint32_t _verdictEventDroppedCount = 0;
    uint32_t _observeFrameCount = 0;
    uint32_t _freshDetectorInputCount = 0;
    uint32_t _detectorDrainCount = 0;
    uint32_t _detectorReportRefreshCount = 0;
    uint32_t _noFreshFrequencySkipCount = 0;
    uint32_t _detectorOccurrencePoppedCount = 0;
    uint32_t _detectorValidOccurrencePoppedCount = 0;
};

// The one build decision: which variant DetectionRuntime holds.
#ifdef ANALYZER_MODE
using ActiveDetectionDiagnostics = DetectionDiagnostics;
#else
using ActiveDetectionDiagnostics = NullDetectionDiagnostics;
#endif

}  // namespace detection
