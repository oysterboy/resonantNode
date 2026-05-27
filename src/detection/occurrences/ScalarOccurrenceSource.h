#pragma once

#include <stdint.h>

#include "../DetectionProfile.h"
#include "../detectors/ScalarTransientDetector.h"
#include "../../io/AudioSignal.h"
#include "Occurrence.h"

namespace detection {

/*
ScalarOccurrenceSource

Owns the reusable scalar candidate lifecycle around ScalarTransientDetector.
It keeps the generic open/peak/release bookkeeping shared by AMP and
frequency sources, while source-specific wrappers only provide the stream
value and fill the payload-specific evidence fields.

Responsibilities:
- track first seen, peak, and release-observed timing for one scalar stream
- translate a closed scalar transient into a Occurrence payload
- keep candidate lifecycle behavior aligned between AMP and frequency sources

Does NOT:
- decide pattern meaning
- own frequency-specific scoring
- own Analyzer SEQ reporting
- own source-specific evidence extraction
*/
class ScalarOccurrenceSource {
public:
    ScalarOccurrenceSource();

    void reset();
    void begin();
    void setConfig(const ScalarTransientConfig& config);
    void observeFrame(const AudioSignalFrame& frame, float signalLevel, OccurrenceKind kind, OccurrenceSource source);

    void setOnsetDetectionThreshold(float value);
    void setOnsetReleaseThreshold(float value);
    void setCooldownAfterOnsetMs(unsigned long value);
    void setMinTransientDurationMs(unsigned long value);
    void setMaxTransientDurationMs(unsigned long value);
    void setMinTransientPeakStrength(float value);
    void setReleaseDebounceMs(unsigned long value);
    void setDiagnosticsEnabled(bool enabled);
    void setDiagnosticsLabel(const char* value);

    void observe(const AudioSignalFrame& frame, float signalLevel);

    bool onsetDetected() const;
    float onsetStrength() const;
    bool transientDetected() const;
    float transientStrength() const;
    unsigned long transientDurationMs() const;
    bool candidateActive() const;
    bool releaseObserved() const;
    unsigned long candidateHoldWindows() const;
    unsigned long candidateFirstSeenMs() const;
    unsigned long candidatePeakMs() const;
    unsigned long candidateReleaseObservedMs() const;
    uint64_t candidateFirstSeenSample() const;
    uint64_t candidatePeakSample() const;
    uint64_t candidateReleaseSample() const;
    float candidatePeakStrength() const;
    const char* lastOnsetRejectReasonName() const;
    const char* lastTransientRejectReasonName() const;
    unsigned long lastTransientRejectedDurationMs() const;
    float lastTransientRejectedStrength() const;

    bool popOccurrence(Occurrence& out);
    bool consumeCandidate(const AudioSignalFrame& frame,
                          OccurrenceKind kind,
                          OccurrenceSource source,
                          Occurrence& out);

private:
    void resetCandidateLifecycle();

    ScalarTransientDetector _detector;
    bool _candidateActive = false;
    bool _releaseObserved = false;
    bool _candidateReady = false;
    bool _hasPending = false;
    uint64_t _candidateFirstSeenSample = 0;
    uint64_t _candidatePeakSample = 0;
    uint64_t _candidateReleaseSample = 0;
    uint32_t _candidateFirstSeenUs = 0;
    uint32_t _candidatePeakUs = 0;
    uint32_t _candidateReleaseObservedUs = 0;
    unsigned long _candidateFirstSeenMs = 0;
    unsigned long _candidatePeakMs = 0;
    unsigned long _candidateReleaseObservedMs = 0;
    unsigned long _candidateHoldWindows = 0;
    float _candidateOnsetStrength = 0.0f;
    float _candidatePeakStrength = 0.0f;
    float _candidateCurrentStrength = 0.0f;
    Occurrence _pending = {};
};

} // namespace detection

