#pragma once

#include <stddef.h>

#include "../io/AudioSignal.h"
#include "DetectionProfile.h"
#include "occurrences/FrequencyOccurrenceSource.h"
#include "occurrences/ScalarOccurrenceSource.h"
#include "inspector/OccurrenceInspector.h"
#include "inspector/InspectorTypes.h"
#include "patterns/PatternAssembler.h"
#include "patterns/PatternRules.h"
#include "patterns/PatternResult.h"
#include "occurrences/Occurrence.h"
#include "occurrences/InspectedOccurrence.h"
#include "field/FieldStateTracker.h"
#include "field/FieldState.h"
#include "features/FeatureExtractor.h"
#include "features/FeatureHistory.h"
#include "features/FrequencyMatchEvaluation.h"

namespace detection {

/*
DetectionRuntime

Owns the active detection pipeline wiring:
feature observation, occurrence emission, occurrence inspection, pattern assembly,
pattern rules, field-state tracking, and PatternResult queueing.

Consumes AudioSignalFrame and FrequencyFeatureFrame.
Produces PatternResult and FieldState.
Does not decide behavior or output.
*/
struct DetectionPipelineResult {
    bool hasPattern = false;
    PatternResult pattern = {};

    bool hasOccurrence = false;
    Occurrence occurrence = {};

    bool hasInspectedOccurrence = false;
    InspectedOccurrence inspectedOccurrence = {};

    bool hasField = false;
    FieldState field = {};

    const char* profileName = "unknown";
    unsigned long timestampMs = 0;
};

struct DetectionDiagnostics {
    unsigned long observedAtMs = 0;

    const char* occurrenceSource = "unknown";
    const char* detectorKind = "unknown";

    bool acceptedPresent = false;
    unsigned long acceptedStartMs = 0;
    unsigned long acceptedPeakMs = 0;
    unsigned long acceptedReleaseMs = 0;
    unsigned long acceptedDurationMs = 0;
    float acceptedStrength = 0.0f;
    float acceptedScore = 0.0f;
    float acceptedContrast = 0.0f;

    unsigned long frequencyFrames = 0;
    unsigned long frequencyValidFrames = 0;
    unsigned long frequencyScoreOkFrames = 0;
    unsigned long frequencyContrastOkFrames = 0;
    unsigned long frequencyBothOkFrames = 0;
    unsigned long frequencyMatchFrames = 0;
    unsigned long frequencyRejectFrames = 0;

    float frequencyScoreMean = 0.0f;
    float frequencyContrastMean = 0.0f;
    float frequencyScoreMin = 0.0f;
    float frequencyContrastMin = 0.0f;
    float frequencyScoreMax = 0.0f;
    float frequencyContrastMax = 0.0f;
    unsigned long frequencyScoreMaxMs = 0;
    unsigned long frequencyContrastMaxMs = 0;
    float frequencyScoreThreshold = 0.0f;
    float frequencyContrastThreshold = 0.0f;

    bool frequencyNearMiss = false;
    const char* frequencyNearMissReason = "none";

    bool frequencyPresent = false;
    bool frequencyValidWindow = false;
    bool frequencyMatched = false;
    bool frequencyScoreOk = false;
    bool frequencyContrastOk = false;
    const char* frequencyRejectReason = "none";
    const char* frequencyNoEmitReason = "none";
    const char* frequencyGateReason = "none";
    const char* frequencyWouldCandidateReason = "none";
    const char* frequencyCandidateState = "none";
    bool frequencyReadyOk = false;
    bool frequencyGateOpen = false;
    bool frequencyOpened = false;
    bool frequencyReleased = false;
    bool frequencyEmitted = false;
    bool frequencyValidRelease = false;
    bool frequencyEmitAllowed = false;
    unsigned long frequencyOpenMs = 0;
    unsigned long frequencyPeakMs = 0;
    unsigned long frequencyReleaseMs = 0;
    unsigned long frequencyDurationMs = 0;
    unsigned long frequencyMinDurationMs = 0;
    unsigned long frequencyMaxDurationMs = 0;

    const char* scalarOnsetRejectReason = "none";
    const char* scalarTransientRejectReason = "none";
    unsigned long scalarTransientRejectedDurationMs = 0;
    float scalarTransientRejectedStrength = 0.0f;

    float ampCenteredMagnitude = 0.0f;
    float ampLevel = 0.0f;
    float ampBaseline = 0.0f;
    float ampLift = 0.0f;
};

class DetectionRuntime {
public:
    DetectionRuntime();

    void resetState();
    void resetDiagnostics();
    void setDiagnosticsEnabled(bool enabled);
    void captureDiagnostics();

    void setFrequencyMatchConfig(const FrequencyMatchConfig& config);
    void setScalarTransientConfig(const ScalarTransientConfig& config);
    void setOccurrenceSource(OccurrenceSourceKind kind);
    void setInspectionPlan(const InspectionPlan& plan);
    void setPatternRulesConfig(const PatternRulesConfig& config);
    void setFieldStateConfig(const FieldStateConfig& config);
    void setProfileName(const char* profileName);

    void observeFrame(
        const AudioSignalFrame& frame,
        const FrequencyFeatureFrame& frequencyEvidence,
        unsigned long nowMs
    );

    bool popPatternResult(PatternResult& out);
    bool hasLatestPipelineResult() const;
    const DetectionPipelineResult& latestPipelineResult() const;
    const DetectionDiagnostics& diagnostics() const;
    const FrequencyOccurrenceSource& frequencyEmitter() const;
    const FieldState& fieldState() const;
    const FeatureHistory& featureHistory() const;

private:
    static constexpr size_t kResultQueueCapacity = 8;

    // Pipeline stages in execution order.
    void drainOccurrenceSources(unsigned long nowMs);
    void drainPatternAssembler(unsigned long nowMs);
    bool pushPatternResult(const PatternResult& result);
    void capturePipelineResult(
        const PatternResult& result,
        const Occurrence* occurrence,
        const InspectedOccurrence* inspectedOccurrence,
        unsigned long nowMs
    );

    FrequencyMatchConfig _frequencyMatchConfig = {};
    ScalarTransientConfig _scalarTransientConfig = {};
    // Profile configuration applied at fixed runtime stages.
    OccurrenceSourceKind _occurrenceSourceKind = OccurrenceSourceKind::FrequencyMatch;
    InspectionPlan _inspectionPlan = {};
    PatternRulesConfig _patternRulesConfig = {};
    const char* _profileName = "unknown";

    FrequencyOccurrenceSource _frequencyEmitter;
    ScalarOccurrenceSource _scalarEmitter;
    OccurrenceInspector _occurrenceInspector;
    PatternAssembler _patternAssembler;
    PatternRules _patternRules;
    FieldStateTracker _fieldStateTracker;
    FeatureHistory _featureHistory;

    PatternResult _resultQueue[kResultQueueCapacity] = {};
    size_t _resultReadIndex = 0;
    size_t _resultCount = 0;

    DetectionPipelineResult _latestPipelineResult = {};
    bool _hasLatestPipelineResult = false;
    DetectionDiagnostics _diagnostics = {};
    bool _diagnosticsEnabled = true;
    Occurrence _lastOccurrence = {};
    InspectedOccurrence _lastInspectedOccurrence = {};
};

} // namespace detection


