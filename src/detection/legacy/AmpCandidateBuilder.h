#pragma once

#include <stddef.h>

#include "../../io/AudioSignal.h"
#include "../detectors/AmpTransientDetector.h"

using AmpCandidate = DetectorCandidate;

/*
AmpCandidateBuilder

Owns the AMP/transient candidate queue and the AMP-side candidate state
transition from sample snapshots into emitted candidate records.

This remains the fallback / comparison candidate owner for AMP.

Responsibilities:
- observe signal snapshots from AudioSignal
- track open/peak/release timing for AMP candidates
- queue finalized AmpCandidate records for later drain

Does NOT:
- read audio directly
- own pattern meaning
- own behavior decisions
- decide tonal validity
*/
class AmpCandidateBuilder {
public:
    void resetState();
    void observeSample(const AudioSignalFrame& frame, const AmpTransientDetector& detector);

    bool popCandidate(AmpCandidate& candidate);
    bool candidateAvailable() const;
    size_t candidateQueueDepth() const;

private:
    static constexpr size_t kCandidateQueueCapacity = 8;

    void finalizeCandidate(const AudioSignalFrame& frame, const AmpTransientDetector& detector);
    bool pushCandidate(const AmpCandidate& candidate);

    AmpCandidate _candidateQueue[kCandidateQueueCapacity] = {};
    size_t _candidateReadIndex = 0;
    size_t _candidateCount = 0;
    bool _candidateActive = false;
    bool _candidateHadOverflow = false;
    uint64_t _candidateOnsetSample = 0;
    uint64_t _candidatePeakSample = 0;
    uint64_t _candidateReleaseSample = 0;
    uint32_t _candidateOnsetMicrosApprox = 0;
    uint32_t _candidateReleaseMicrosApprox = 0;
    uint32_t _candidateOnsetMillisApprox = 0;
    uint32_t _candidateReleaseMillisApprox = 0;
    float _candidatePeakStrength = 0.0f;
    float _candidateOnsetStrength = 0.0f;
    float _candidateReleaseStrength = 0.0f;
    float _candidateAmbientBaseline = 0.0f;
};
