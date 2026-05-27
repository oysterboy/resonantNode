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
    bool present = false;
    unsigned long observedAtMs = 0;

    const char* occurrenceSource = "unknown";
    const char* detectorKind = "unknown";

    bool frequencyPresent = false;
    bool frequencyValidWindow = false;
    bool frequencyMatched = false;
    bool frequencyScoreOk = false;
    bool frequencyContrastOk = false;
    float frequencyScore = 0.0f;
    float frequencyContrast = 0.0f;
    float frequencyScoreMin = 0.0f;
    float frequencyContrastMin = 0.0f;
    const char* frequencyReason = "none";
    const char* frequencySuppressReason = "none";
    const char* frequencyWouldCandidateReason = "none";
    const char* frequencyCandidateState = "none";
    bool frequencyReadyOk = false;
    bool frequencyGateOpen = false;

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
    void updateDiagnostics(
        const AudioSignalFrame& frame,
        const FrequencyFeatureFrame& frequencyEvidence,
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
    Occurrence _lastOccurrence = {};
    InspectedOccurrence _lastInspectedOccurrence = {};
};

} // namespace detection


