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
#include "patterns/PatternMatcher.h"
#include "patterns/PatternResult.h"
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
feature observation, occurrence emission, occurrence inspection, pattern
matching, field-state tracking, and PatternResult queueing.

Consumes AudioSamplePacket and FrequencyBandMeasurementPacket.
Produces PatternResult and FieldState.
Does not decide behavior or output.
Feature producers fan out fresh samples to FeatureHistory and the selected
detector path in parallel; FeatureHistory is for retrospective inspection, not
a live pipe into occurrence emission.
*/
struct DetectionPipelineResult {
    bool hasPattern = false;
    PatternResult pattern = {};
    bool hasPatternReport = false;
    PatternMatcherReport patternReport = {};

    bool hasPatternInspectedOccurrence = false;
    InspectedOccurrence patternInspectedOccurrence = {};

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
    unsigned long sourceOccurrenceId = 0;
    unsigned long sourceCandidateId = 0;
    bool sourceReportMatched = false;
};

enum class PipelineIntegrityReason {
    None,
    MissingDetectorReport,
    MissingInspectedOccurrence,
    MissingPatternResult,
    OccurrenceIdMismatch,
    InspectionQueueOverflow,
    PatternResultQueueOverflow,
    PipelineEventQueueOverflow,
};

struct PipelineIntegrity {
    bool detectorReportPresent = false;
    bool occurrenceMatched = false;
    bool inspectionPresent = false;
    bool patternReportPresent = false;
    bool patternResultPresent = false;
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

    bool hasPatternResult = false;
    PatternResult patternResult = {};

    bool hasInspectedOccurrence = false;
    InspectedOccurrence inspectedOccurrence = {};

    bool hasSourceRecord = false;
    SourceDiagnosticRecord sourceRecord = {};
};

class DetectionRuntime {
public:
    DetectionRuntime();

    void resetState();
    void resetDiagnostics();
    void resetDiagnosticsCounters();
    void resetDetectors();
    void resetSourceRejectSummaries();
    void resetDetectionState();
    void setDiagnosticsEnabled(bool enabled);

    void setFrequencyMatchConfig(const FrequencyMatchConfig& config);
    void setScalarTransientConfig(const ScalarTransientConfig& config);
    void setDetectorSelection(DetectorSelection selection);
    void setInspectionPlan(const InspectionPlan& plan);
    void setFieldStateConfig(const FieldStateConfig& config);
    void setProfileName(const char* profileName);

    void observeFrame(
        const AudioSamplePacket& audioSamplePacket,
        const FrequencyBandMeasurementPacket& frequencyEvidence,
        unsigned long nowMs
    );

    bool popPipelineEvent(DetectionPipelineEvent& out);
    bool popPatternResult(PatternResult& out);
    bool hasLatestPipelineResult() const;
    const DetectionPipelineResult& latestPipelineResult() const;
    unsigned long pipelineEventOverflowCount() const;
    unsigned long patternResultQueueOverflowCount() const;
    unsigned long patternInspectedQueueOverflowCount() const;
    unsigned long patternCorrelationFailureCount() const;
    // Generic report access is the canonical upward path.
    const DetectorReport& activeDetectorReport() const;
    const PatternMatcherReport& activePatternMatcherReport() const;
    const FieldState& fieldState() const;
    const FeatureHistory& featureHistory() const;

private:
    static constexpr size_t kPipelineEventQueueCapacity = 4;
    static constexpr size_t kResultQueueCapacity = 4;

    // Pipeline stages in execution order.
    bool pushPipelineEvent(const DetectionPipelineEvent& event);
    void drainDetectors(unsigned long nowMs);
    void drainPatternMatcher(unsigned long nowMs);
    bool pushPatternResult(const PatternResult& result);
    bool pushPatternInspectedOccurrence(const InspectedOccurrence& occurrence);
    bool popPatternInspectedOccurrence(unsigned long occurrenceId, InspectedOccurrence& out);
    void capturePipelineResult(
        const PatternResult& result,
        const InspectedOccurrence* matchedInspectedOccurrence,
        unsigned long nowMs
    );
    void refreshDetectorReports(unsigned long nowMs);

    FrequencyMatchConfig _frequencyMatchConfig = {};
    ScalarTransientConfig _scalarTransientConfig = {};
    // Profile configuration applied at fixed runtime stages.
    DetectorSelection _detectorSelection = DetectorSelection::FrequencyMatch;
    InspectionPlan _inspectionPlan = {};
    const char* _profileName = "unknown";

    FrequencyMatchDetector _frequencyDetector;
    ScalarTransientDetector _scalarDetector;
    OccurrenceInspector _occurrenceInspector;
    PatternMatcher _patternMatcher;
    FieldStateTracker _fieldStateTracker;
    FeatureHistory _featureHistory;

    PatternResult _resultQueue[kResultQueueCapacity] = {};
    size_t _resultReadIndex = 0;
    size_t _resultCount = 0;
    unsigned long _patternResultQueueOverflowCount = 0;

    DetectionPipelineResult _latestPipelineResult = {};
    bool _hasLatestPipelineResult = false;
    DetectorReport _detectorReport = {};
    unsigned long _pipelineEventOverflowCount = 0;
    DetectionPipelineEvent _pipelineEventQueue[kPipelineEventQueueCapacity] = {};
    size_t _pipelineEventReadIndex = 0;
    size_t _pipelineEventCount = 0;
    unsigned long _pipelineEventSequenceId = 0;
    uint32_t _lastEmittedAcceptedOccurrenceId = 0;
    uint32_t _lastEmittedSelectedRejectOccurrenceId = 0;
    InspectedOccurrence _patternInspectedQueue[kResultQueueCapacity] = {};
    size_t _patternInspectedReadIndex = 0;
    size_t _patternInspectedCount = 0;
    unsigned long _patternInspectedQueueOverflowCount = 0;
    unsigned long _patternCorrelationFailureCount = 0;
};

} // namespace detection


