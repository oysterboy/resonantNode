#pragma once

#include <stddef.h>

#include "../io/AudioSignal.h"
#include "signals/AmpSignalEmitter.h"
#include "signals/FrequencySignalEmitter.h"
#include "inspector/SignalInspector.h"
#include "inspector/InspectorTypes.h"
#include "patterns/PatternAssembler.h"
#include "patterns/PatternRules.h"
#include "patterns/PatternResult.h"
#include "signals/SignalCandidate.h"
#include "signals/InspectedSignal.h"
#include "field/FieldStateTracker.h"
#include "field/FieldState.h"
#include "features/FeatureExtractor.h"
#include "features/FeatureHistory.h"
#include "features/FrequencyMatchEvaluation.h"

namespace detection {

struct DetectionPipelineResult {
    bool hasPattern = false;
    PatternResult pattern = {};

    bool hasSignal = false;
    SignalCandidate signal = {};

    bool hasInspectedSignal = false;
    InspectedSignal inspectedSignal = {};

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
    void setSignalEmitter(ProfileSignalEmitterKind kind);
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
    const FrequencySignalEmitter& frequencyEmitter() const;
    const FieldState& fieldState() const;
    const FeatureHistory& featureHistory() const;

private:
    static constexpr size_t kResultQueueCapacity = 8;

    void drainSignalEmitters(unsigned long nowMs);
    void drainPatternAssembler(unsigned long nowMs);
    bool pushPatternResult(const PatternResult& result);
    void capturePipelineResult(
        const PatternResult& result,
        const SignalCandidate* signal,
        const InspectedSignal* inspectedSignal,
        unsigned long nowMs
    );

    FrequencyMatchEvaluation::Values _frequencyTuning = {};
    ProfileSignalEmitterKind _signalEmitterKind = ProfileSignalEmitterKind::Frequency;
    ProfileInspectionRulesKind _inspectionRulesKind = ProfileInspectionRulesKind::FreqAmp;
    InspectionConfig _inspectionConfig = defaultInspectionConfig();
    PatternRulesConfig _patternRulesConfig = {};
    const char* _profileName = "unknown";

    AmpSignalEmitter _ampEmitter;
    FrequencySignalEmitter _frequencyEmitter;
    SignalInspector _signalInspector;
    PatternAssembler _patternAssembler;
    PatternRules _patternRules;
    FieldStateTracker _fieldStateTracker;
    FeatureHistory _featureHistory;

    PatternResult _resultQueue[kResultQueueCapacity] = {};
    size_t _resultReadIndex = 0;
    size_t _resultCount = 0;

    DetectionPipelineResult _latestPipelineResult = {};
    bool _hasLatestPipelineResult = false;
    SignalCandidate _lastSignalCandidate = {};
    InspectedSignal _lastInspectedSignal = {};
};

} // namespace detection
