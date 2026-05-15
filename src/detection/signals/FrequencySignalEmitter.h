#pragma once

#include "SignalCandidate.h"

class AudioSignalFrame;
class FreqTransientDetector;
class FrequencyCandidateBuilder;

namespace detection {

class FrequencySignalEmitter {
public:
    FrequencySignalEmitter();

    void reset();

    void observeFrame(
        const AudioSignalFrame& frame,
        const DetectionPipeline::FrequencyEvidence& evidence,
        FreqTransientDetector& detector,
        FrequencyCandidateBuilder& builder
    );

    bool popSignalCandidate(SignalCandidate& out);

private:
    bool _hasPending = false;
    uint64_t _lastEmittedReleaseSample = 0;
    SignalCandidate _pending = {};
};

} // namespace detection
