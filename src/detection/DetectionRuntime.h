#pragma once

#include <stddef.h>

#include "../io/AudioSignal.h"
#include "signals/AmpSignalEmitter.h"
#include "signals/FrequencySignalEmitter.h"
#include "signals/SignalInspector.h"
#include "patterns/PatternAssembler.h"
#include "patterns/PatternRules.h"
#include "patterns/PatternResult.h"
#include "field/FieldStateTracker.h"
#include "FrequencyEvidenceEvaluation.h"

namespace detection {

class DetectionRuntime {
public:
    DetectionRuntime();

    void reset();

    void setFrequencyTuning(const FrequencyEvidenceEvaluation::Values& tuning);
    void setAmpEnabled(bool enabled);

    void observeFrame(
        const AudioSignalFrame& frame,
        const DetectionPipeline::FrequencyEvidence& frequencyEvidence,
        unsigned long nowMs
    );

    bool popPatternResult(PatternResult& out);
    const FrequencySignalEmitter& frequencyEmitter() const;
    const FieldState& fieldState() const;

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

    PatternResult _resultQueue[kResultQueueCapacity] = {};
    size_t _resultReadIndex = 0;
    size_t _resultCount = 0;
};

} // namespace detection
