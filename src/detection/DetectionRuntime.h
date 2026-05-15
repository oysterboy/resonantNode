#pragma once

#include <stddef.h>

#include "../io/AudioSignal.h"
#include "signals/AmpSignalEmitter.h"
#include "signals/FrequencySignalEmitter.h"
#include "inspector/SignalInspector.h"
#include "patterns/PatternAssembler.h"
#include "patterns/PatternRules.h"
#include "patterns/PatternPayload.h"
#include "field/FieldStateTracker.h"
#include "features/FeatureExtractor.h"
#include "features/FeatureHistory.h"
#include "inspector/FrequencyEvidenceEvaluation.h"

namespace detection {

class DetectionRuntime {
public:
    DetectionRuntime();

    void reset();

    void setFrequencyTuning(const FrequencyEvidenceEvaluation::Values& tuning);
    void setAmpEnabled(bool enabled);
    void setFieldStateConfig(const FieldStateConfig& config);

    void observeFrame(
        const AudioSignalFrame& frame,
        const FrequencyEvidence& frequencyEvidence,
        unsigned long nowMs
    );

    bool popPatternResult(PatternResult& out);
    const FrequencySignalEmitter& frequencyEmitter() const;
    const FieldState& fieldState() const;
    const FeatureHistory& featureHistory() const;

private:
    static constexpr size_t kResultQueueCapacity = 8;

    void drainSignalEmitters(unsigned long nowMs);
    void drainPatternAssembler(unsigned long nowMs);
    bool pushPatternResult(const PatternResult& result);

    FrequencyEvidenceEvaluation::Values _frequencyTuning = {};
    bool _ampEnabled = true;

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
};

} // namespace detection
