#pragma once

#include <stddef.h>

// Keep the canonical detector-report contract compiled through the active
// runtime header chain. DetectionRuntime snapshots scalar and frequency
// detector reports for analyzer/report consumers.
#include "detectors/DetectorReport.h"
#include "../audio/AudioSignal.h"
#include "DetectionProfile.h"
#include "detectors/frequency/FrequencyMatchDetector.h"
#include "detectors/scalar/ScalarTransientDetector.h"
#include "inspection/OccurrenceInspector.h"
#include "inspection/InspectorTypes.h"
#include "evaluation/OccurrenceEvaluator.h"
#include "evaluation/OccurrenceVerdict.h"
#include "occurrences/Occurrence.h"
#include "occurrences/InspectedOccurrence.h"
#include "field/FieldStateTracker.h"
#include "field/FieldState.h"
#include "features/FeatureExtractor.h"
#include "features/FeatureHistory.h"
#include "detectors/frequency/FrequencyMatchCriteria.h"

namespace detection {

/*
DetectionRuntime

Owns the active detection pipeline wiring:
feature observation, occurrence emission, occurrence inspection, occurrence
evaluation, field-state tracking, and OccurrenceVerdict queueing.

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
    bool hasEvaluatorReport = false;
    OccurrenceEvaluatorReport evaluatorReport = {};

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

struct PendingVerdictObservation {
    InspectedOccurrence inspected = {};
    DetectorReport detectorReport = {};
};

enum class PipelineIntegrityReason {
    None,
    MissingDetectorReport,
    MissingInspectedOccurrence,
    MissingVerdict,
    OccurrenceIdMismatch,
    InspectionQueueOverflow,
    VerdictQueueOverflow,
    PipelineEventQueueOverflow,
};

struct PipelineIntegrity {
    bool detectorReportPresent = false;
    bool occurrenceMatched = false;
    bool inspectionPresent = false;
    bool evaluatorReportPresent = false;
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

    void setFrequencyMatchConfig(const FrequencyMatchConfig& config);
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
    unsigned long verdictQueueOverflowCount() const;
    unsigned long verdictCorrelationQueueOverflowCount() const;
    unsigned long verdictCorrelationFailureCount() const;
    unsigned long detectorReportMismatchCount() const;
    uint32_t observeFrameCount() const;
    uint32_t freshDetectorInputCount() const;
    uint32_t detectorDrainCount() const;
    uint32_t evaluatorDrainCount() const;
    uint32_t detectorReportRefreshCount() const;
    uint32_t noFreshFrequencySkipCount() const;
    uint32_t detectorOccurrencePoppedCount() const;
    uint32_t detectorValidOccurrencePoppedCount() const;
    uint32_t evaluatorAcceptAttemptCount() const;
    uint32_t evaluatorAcceptSuccessCount() const;
    uint32_t evaluatorAcceptRejectCount() const;
    uint32_t verdictProducedCount() const;
    uint32_t verdictEventPushedCount() const;
    uint32_t verdictEventDroppedCount() const;
    EvaluatorInputRejectReason latestEvaluatorInputRejectReason() const;
    uint32_t scalarReportGeneration() const;
    uint32_t frequencyReportGeneration() const;
    // Generic report access is the canonical upward path.
    const DetectorReport& activeDetectorReport() const;
    const OccurrenceEvaluatorReport& activeEvaluatorReport() const;
    const FeatureHistory& featureHistory() const;
#endif

private:
    static constexpr size_t kResultQueueCapacity = 4;
#ifdef ANALYZER_MODE
    static constexpr size_t kPipelineEventQueueCapacity = 4;
#endif

    // Pipeline stages in execution order. drainDetectors()/drainOccurrenceEvaluator()
    // stay core (they produce OccurrenceVerdict/FieldState); each has internal
    // ANALYZER_MODE blocks around the diagnostics-only work interleaved in
    // their loop bodies, see the .cpp file.
    void drainDetectors(unsigned long nowMs);
    void drainOccurrenceEvaluator(unsigned long nowMs);
    bool pushOccurrenceVerdict(const OccurrenceVerdict& result);
    bool hasPendingDetectorOutput() const;
    bool hasPendingEvaluatorWork() const;
    void resetDetectionQueues();

#ifdef ANALYZER_MODE
    bool pushPipelineEvent(const DetectionPipelineEvent& event);
    bool pushVerdictObservation(const PendingVerdictObservation& observation);
    bool popVerdictObservation(unsigned long occurrenceId, PendingVerdictObservation& out);
    bool captureLatestDetectorReportIfChanged();
    void drainDetectorReportEvents(unsigned long nowMs);
    bool capturePipelineResult(
        const OccurrenceVerdict& result,
        const InspectedOccurrence* matchedInspectedOccurrence,
        const DetectorReport* matchedDetectorReport,
        unsigned long nowMs
    );
    void resetDetectionBookkeeping();
#endif

    FrequencyMatchConfig _frequencyMatchConfig = {};
    ScalarTransientConfig _scalarTransientConfig = {};
    // Profile configuration applied at fixed runtime stages.
    DetectorSelection _detectorSelection = DetectorSelection::FrequencyMatch;
    InspectionPlan _inspectionPlan = {};
    // Written by the core setProfileName() Node calls; read only by the
    // Analyzer-only capturePipelineResult() below. Kept as a plain core
    // member rather than gated: it is a single pointer with a trivial
    // always-executed assignment, not the queue/counter/logic weight this
    // split targets.
    const char* _profileName = "unknown";

    FrequencyMatchDetector _frequencyDetector;
    ScalarTransientDetector _scalarDetector;
    OccurrenceInspector _occurrenceInspector;
    OccurrenceEvaluator _occurrenceEvaluator;
    FieldStateTracker _fieldStateTracker;
    FeatureHistory _featureHistory;

    OccurrenceVerdict _resultQueue[kResultQueueCapacity] = {};
    size_t _resultReadIndex = 0;
    size_t _resultCount = 0;
    bool _verdictQueueEnabled = true;

#ifdef ANALYZER_MODE
    unsigned long _verdictQueueOverflowCount = 0;

    DetectionPipelineResult _latestPipelineResult = {};
    bool _hasLatestPipelineResult = false;
    DetectorReport _detectorReport = {};
    uint32_t _lastObservedScalarReportGeneration = 0;
    uint32_t _lastObservedFrequencyReportGeneration = 0;
    unsigned long _pipelineEventOverflowCount = 0;
    unsigned long _detectorReportMismatchCount = 0;
    DetectionPipelineEvent _pipelineEventQueue[kPipelineEventQueueCapacity] = {};
    size_t _pipelineEventReadIndex = 0;
    size_t _pipelineEventCount = 0;
    unsigned long _pipelineEventSequenceId = 0;
    uint32_t _lastEmittedAcceptedOccurrenceId = 0;
    uint32_t _lastEmittedAcceptedReportGeneration = 0;
    uint32_t _lastEmittedSelectedRejectOccurrenceId = 0;
    uint32_t _lastEmittedSelectedRejectReportGeneration = 0;
    PendingVerdictObservation _verdictCorrelationQueue[kResultQueueCapacity] = {};
    size_t _verdictCorrelationReadIndex = 0;
    size_t _verdictCorrelationCount = 0;
    unsigned long _verdictCorrelationQueueOverflowCount = 0;
    unsigned long _verdictCorrelationFailureCount = 0;
    EvaluatorInputRejectReason _latestEvaluatorInputRejectReason = EvaluatorInputRejectReason::None;
    uint32_t _evaluatorAcceptAttemptCount = 0;
    uint32_t _evaluatorAcceptSuccessCount = 0;
    uint32_t _evaluatorAcceptRejectCount = 0;
    uint32_t _verdictProducedCount = 0;
    uint32_t _verdictEventPushedCount = 0;
    uint32_t _verdictEventDroppedCount = 0;

    uint32_t _observeFrameCount = 0;
    uint32_t _freshDetectorInputCount = 0;
    uint32_t _detectorDrainCount = 0;
    uint32_t _evaluatorDrainCount = 0;
    uint32_t _detectorReportRefreshCount = 0;
    uint32_t _noFreshFrequencySkipCount = 0;
    uint32_t _detectorOccurrencePoppedCount = 0;
    uint32_t _detectorValidOccurrencePoppedCount = 0;
#endif
};

} // namespace detection


