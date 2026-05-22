#pragma once

#include <stddef.h>

#include "../io/AudioSignal.h"
#include "occurrences/AmpOccurrenceSource.h"
#include "occurrences/FrequencyOccurrenceSource.h"
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

Consumes AudioSignalFrame and FrequencyEvidence.
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

class DetectionRuntime {
public:
    DetectionRuntime();

    void reset();

    void setFrequencyTuning(const FrequencyMatchEvaluation::Values& tuning);
    void setOccurrenceSource(ProfileOccurrenceSourceKind kind);
    void setInspectionRules(ProfileInspectionRulesKind kind);
    void setInspectionConfig(const InspectionConfig& config);
    void setPatternRulesConfig(const PatternRulesConfig& config);
    void setFieldStateConfig(const FieldStateConfig& config);
    void setProfileName(const char* profileName);

    void observeFrame(
        const AudioSignalFrame& frame,
        const FrequencyEvidence& frequencyEvidence,
        unsigned long nowMs
    );

    bool popPatternResult(PatternResult& out);
    bool hasLatestPipelineResult() const;
    const DetectionPipelineResult& latestPipelineResult() const;
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

    FrequencyMatchEvaluation::Values _frequencyTuning = {};
    // Profile configuration applied at fixed runtime stages.
    ProfileOccurrenceSourceKind _occurrenceSourceKind = ProfileOccurrenceSourceKind::Frequency;
    ProfileInspectionRulesKind _inspectionRulesKind = ProfileInspectionRulesKind::TonalPulse;
    InspectionConfig _inspectionConfig = defaultInspectionConfig();
    PatternRulesConfig _patternRulesConfig = {};
    const char* _profileName = "unknown";

    AmpOccurrenceSource _ampEmitter;
    FrequencyOccurrenceSource _frequencyEmitter;
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
    Occurrence _lastOccurrence = {};
    InspectedOccurrence _lastInspectedOccurrence = {};
};

} // namespace detection


