#pragma once

#include <stddef.h>

// Keep the canonical detector-report contract compiled through the active
// runtime header chain. DetectionRuntime snapshots the scalar detector's
// reports for analyzer/report consumers.
#include "detectors/DetectorReport.h"
#include "../audio/AudioSignal.h"
#include "DetectionProfile.h"
#include "detectors/scalar/ScalarTransientDetector.h"
#include "inspection/OccurrenceInspector.h"
#include "inspection/InspectorTypes.h"
#include "inspection/VerdictProjection.h"
#include "evaluation/OccurrenceVerdict.h"
#include "occurrences/Occurrence.h"
#include "occurrences/InspectedOccurrence.h"
#include "field/FieldStateTracker.h"
#include "field/FieldState.h"
#include "features/FeatureExtractor.h"
#include "features/FeatureHistory.h"

namespace detection {

/*
DetectionRuntime

Owns the active detection pipeline wiring:
feature observation, occurrence emission, occurrence inspection (which ends
in the OccurrenceVerdict, makeVerdict()), field-state tracking, and
OccurrenceVerdict queueing.

Consumes AudioSamplePacket and FrequencyBandMeasurementPacket.
Produces OccurrenceVerdict and FieldState.
Does not decide behavior or output.
Feature producers fan out fresh samples to FeatureHistory and the selected
detector path in parallel; FeatureHistory is for retrospective inspection, not
a live pipe into occurrence emission.
*/
struct DetectionPipelineResult {
    bool hasVerdict = false;
    OccurrenceVerdict verdict = {};

    bool hasVerdictInspectedOccurrence = false;
    InspectedOccurrence verdictInspectedOccurrence = {};

    bool hasOccurrence = false;
    Occurrence occurrence = {};

    bool hasField = false;
    FieldState field = {};

    const char* profileName = "unknown";
    unsigned long timestampMs = 0;
};

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

class DetectionRuntime {
public:
    DetectionRuntime();

    // Core, Node-required surface. Every method below this point and above
    // the ANALYZER_MODE block is called by ResonantNodeApp today (confirmed
    // by search) and must keep working, unchanged in shape, in every build.
    void resetState();
    void resetDetectors();
    void resetDetectionState();

    void setScalarTransientConfig(const ScalarTransientConfig& config);
    void setDetectorSelection(DetectorSelection selection);
    void setInspectionPlan(const InspectionPlan& plan);
    void setFieldStateConfig(const FieldStateConfig& config);
    void setProfileName(const char* profileName);
    void setVerdictQueueEnabled(bool enabled);

    void observeFrame(
        const AudioSamplePacket& audioSamplePacket,
        const FrequencyBandMeasurementPacket& frequencyEvidence,
        unsigned long nowMs
    );

    bool popOccurrenceVerdict(OccurrenceVerdict& out);
    const FieldState& fieldState() const;
    // Verdicts dropped because the 4-slot queue was full (every build since
    // step 8b; the Node prints it in RB STATUS). Reset with the queues.
    unsigned long verdictQueueOverflowCount() const;
    // Occurrences inspected early because the 2-slot wait queue was full
    // (step 8b item 1; 0 on the shipped values).
    unsigned long pendingInspectionOverflowCount() const;

    // Analyzer-only diagnostics surface below. None of this is called from
    // ResonantNodeApp or EmitterApp (confirmed by search); it exists to
    // serve AnalyzerSystemReporter.cpp/AnalyzerSequenceSession.cpp/
    // AnalyzerModeApp.cpp/AnalyzerTrialCapture.cpp/AnalyzerRuntimeReporter.cpp
    // alone. Compiled out entirely for env:esp32dev/env:esp32dev-emitter, not
    // merely unreachable: see cleanup-analyzer-node-isolation.md.
#ifdef ANALYZER_MODE
    void resetDiagnostics();
    void resetDiagnosticsCounters();
    void resetSourceRejectSummaries();
    void setDiagnosticsEnabled(bool enabled);

    bool popPipelineEvent(DetectionPipelineEvent& out);
    bool hasLatestPipelineResult() const;
    const DetectionPipelineResult& latestPipelineResult() const;
    unsigned long pipelineEventOverflowCount() const;
    unsigned long detectorReportMismatchCount() const;
    uint32_t observeFrameCount() const;
    uint32_t freshDetectorInputCount() const;
    uint32_t detectorDrainCount() const;
    uint32_t detectorReportRefreshCount() const;
    uint32_t noFreshFrequencySkipCount() const;
    uint32_t detectorOccurrencePoppedCount() const;
    uint32_t detectorValidOccurrencePoppedCount() const;
    uint32_t verdictProducedCount() const;
    uint32_t verdictEventPushedCount() const;
    uint32_t verdictEventDroppedCount() const;
    uint32_t scalarReportGeneration() const;
    // Generic report access is the canonical upward path.
    const DetectorReport& activeDetectorReport() const;
    const FeatureHistory& featureHistory() const;

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
    const HistoryIncompleteSnapshot& historyIncompleteSnapshot() const;
#endif

private:
    static constexpr size_t kResultQueueCapacity = 4;
#ifdef ANALYZER_MODE
    static constexpr size_t kPipelineEventQueueCapacity = 4;
#endif

    // drainDetectors() stays core: it pops occurrences, inspects them and
    // renders their verdicts (OccurrenceVerdict/FieldState), with
    // ANALYZER_MODE blocks around the diagnostics-only work in its loop body.
    void drainDetectors(unsigned long nowMs);
    void inspectAndRender(const Occurrence& occurrence, unsigned long nowMs
#ifdef ANALYZER_MODE
        , const DetectorReport& detectorReport
#endif
    );
    bool pushOccurrenceVerdict(const OccurrenceVerdict& result);
    bool hasPendingDetectorOutput() const;
    void resetDetectionQueues();

#ifdef ANALYZER_MODE
    bool pushPipelineEvent(const DetectionPipelineEvent& event);
    bool captureLatestDetectorReportIfChanged();
    void drainDetectorReportEvents(unsigned long nowMs);
    bool capturePipelineResult(
        const OccurrenceVerdict& result,
        const InspectedOccurrence& inspected,
        const DetectorReport& detectorReport,
        unsigned long nowMs
    );
    void resetDetectionBookkeeping();
#endif

    ScalarTransientConfig _scalarTransientConfig = {};
    // Profile configuration applied at fixed runtime stages.
    DetectorSelection _detectorSelection = DetectorSelection::ScalarTransient;
    InspectionPlan _inspectionPlan = {};
    // Written by the core setProfileName() Node calls; read only by the
    // Analyzer-only capturePipelineResult() below. Kept as a plain core
    // member rather than gated: it is a single pointer with a trivial
    // always-executed assignment, not the queue/counter/logic weight this
    // split targets.
    const char* _profileName = "unknown";

    ScalarTransientDetector _scalarDetector;
    OccurrenceInspector _occurrenceInspector;
    FieldStateTracker _fieldStateTracker;
    FeatureHistory _featureHistory;

    OccurrenceVerdict _resultQueue[kResultQueueCapacity] = {};
    size_t _resultReadIndex = 0;
    size_t _resultCount = 0;
    bool _verdictQueueEnabled = true;
    unsigned long _verdictQueueOverflowCount = 0;

    // Occurrences waiting for their inspection window to complete (step 8b
    // item 1). The detector report is kept with each for the Analyzer event.
    struct PendingInspection {
        Occurrence occurrence = {};
        unsigned long readyAtMs = 0;
#ifdef ANALYZER_MODE
        DetectorReport detectorReport = {};
#endif
    };
    static constexpr size_t kPendingInspectionCapacity = 2;
    PendingInspection _pendingInspections[kPendingInspectionCapacity] = {};
    size_t _pendingInspectionReadIndex = 0;
    size_t _pendingInspectionCount = 0;
    unsigned long _pendingInspectionOverflowCount = 0;

#ifdef ANALYZER_MODE

    DetectionPipelineResult _latestPipelineResult = {};
    bool _hasLatestPipelineResult = false;
    DetectorReport _detectorReport = {};
    uint32_t _lastObservedScalarReportGeneration = 0;
    unsigned long _pipelineEventOverflowCount = 0;
    unsigned long _detectorReportMismatchCount = 0;
    HistoryIncompleteSnapshot _historyIncomplete = {};
    DetectionPipelineEvent _pipelineEventQueue[kPipelineEventQueueCapacity] = {};
    size_t _pipelineEventReadIndex = 0;
    size_t _pipelineEventCount = 0;
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
#endif
};

} // namespace detection


