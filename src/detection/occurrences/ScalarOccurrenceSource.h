#pragma once

#include <stdint.h>

#include "../DetectionProfile.h"
#include "../detectors/ScalarTransientDetector.h"
#include "../../io/AudioSignal.h"
#include "Occurrence.h"

namespace detection {

/*
ScalarOccurrenceSource

Temporary migration wrapper around the canonical ScalarTransientDetector core.
Now acts as a temporary compatibility shell around ScalarTransientDetector.
It retains legacy scalar reject-summary bookkeeping while accepted scalar
Occurrence emission moves into the detector core.

Responsibilities:
- track first seen, peak, and release-observed timing for one scalar stream
- keep legacy reject aggregates aligned with detector state
- forward scalar stream observations into the detector core

Does NOT:
- decide pattern meaning
- own frequency-specific scoring
- own canonical DetectorReport truth for scalar accepted/detail/reject fields
- own accepted scalar Occurrence construction
- own Analyzer SEQ reporting
- own source-specific evidence extraction
- define the final public detector boundary
*/
class ScalarOccurrenceSource {
public:
    ScalarOccurrenceSource();

    void reset();
    void resetRejectSummary();
    void begin();
    void setConfig(const ScalarTransientConfig& config);
    void observeFrame(const AudioSamplePacket& audioSamplePacket, float signalLevel, OccurrenceKind kind, OccurrenceSource source);

    void setOnsetDetectionThreshold(float value);
    void setOnsetReleaseThreshold(float value);
    void setCooldownAfterOnsetMs(unsigned long value);
    void setMinTransientDurationMs(unsigned long value);
    void setMaxTransientDurationMs(unsigned long value);
    void setMinTransientPeakStrength(float value);
    void setReleaseDebounceMs(unsigned long value);
    void setDiagnosticsEnabled(bool enabled);
    void setDiagnosticsLabel(const char* value);

    void observe(
        const AudioSamplePacket& audioSamplePacket,
        float signalLevel,
        OccurrenceKind kind,
        OccurrenceSource source
    );
    ScalarTransientDetector& detector();
    const ScalarTransientDetector& detector() const;

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
    unsigned long rejectedCandidateCount() const;
    unsigned long bestRejectedDurationMs() const;
    unsigned long secondBestRejectedDurationMs() const;
    unsigned long bestRejectedOpenMs() const;
    unsigned long bestRejectedPeakMs() const;
    unsigned long bestRejectedLastMatchMs() const;
    unsigned long bestRejectedCloseMs() const;
    float bestRejectedPeakStrength() const;
    float maxRejectedPeakStrength() const;
    unsigned long maxRejectedPeakStrengthMs() const;
    const char* bestRejectedReasonName() const;
    const char* bestRejectedGateReasonName() const;
    unsigned long totalRejectedMatchMs() const;
    unsigned long totalRejectedGapMs() const;
    unsigned long maxRejectedGapMs() const;
    unsigned long rejectedIslandCount() const;
    const char* lastOnsetRejectReasonName() const;
    const char* lastTransientRejectReasonName() const;
    unsigned long lastTransientRejectedDurationMs() const;
    float lastTransientRejectedStrength() const;

    bool popOccurrence(Occurrence& out);

private:
    void resetCandidateLifecycle();

    ScalarTransientDetector _detector;
    bool _candidateActive = false;
    bool _releaseObserved = false;
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
    unsigned long _rejectedCandidateCount = 0;
    unsigned long _rejectedBestDurationMs = 0;
    unsigned long _rejectedSecondBestDurationMs = 0;
    unsigned long _rejectedBestOpenMs = 0;
    unsigned long _rejectedBestPeakMs = 0;
    unsigned long _rejectedBestLastMatchMs = 0;
    unsigned long _rejectedBestCloseMs = 0;
    float _rejectedBestPeakStrength = 0.0f;
    float _rejectedMaxPeakStrength = 0.0f;
    unsigned long _rejectedMaxPeakStrengthMs = 0;
    const char* _rejectedBestReason = "none";
    const char* _rejectedBestGateReason = "none";
    unsigned long _rejectedTotalMatchMs = 0;
    unsigned long _rejectedTotalGapMs = 0;
    unsigned long _rejectedMaxGapMs = 0;
    unsigned long _lastRejectedCloseMs = 0;
    unsigned long _rejectedIslandCount = 0;
    unsigned long _lastObservedTransientRejectedCount = 0;
};

} // namespace detection

