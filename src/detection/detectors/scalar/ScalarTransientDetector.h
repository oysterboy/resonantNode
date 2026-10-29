#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "../../../app/RuntimeDefaults.h"
#include "../../../audio/AudioSignal.h"
#include "../DetectorReport.h"
#include "ScalarTransientConfig.h"
#include "../../occurrences/Occurrence.h"

namespace detection::scalar_transient_detail {

inline bool detectorReasonIsNone(const char* reason) {
    return reason == nullptr || strcmp(reason, "none") == 0;
}

} // namespace detection::scalar_transient_detail

/*
ScalarTransientDetector

Owns the reusable scalar-stream gate and transient lifecycle.
This is the detector-core implementation for scalar evidence, not a public
behavior boundary.

Responsibilities:
- derive one-shot onset events from a scalar evidence stream
- hold a peak open until release conditions are stable
- own best-rejected lifecycle reporting for the active trial window
- emit accepted Occurrence values for inspector/pattern/analyzer consumers
- qualify peaks as transient events after measuring duration and strength

Does NOT:
- decide behavior or output timing
- own behavior state transitions
- own the scalar stream extraction itself
- own frequency-specific scoring logic
*/
class ScalarTransientDetector {
public:
    // Scalar keeps typed internal reject reasons to make the lifecycle easy
    // to inspect and keep on par with the detector-report snapshot.
    enum class TransientRejectReason {
        None,
        DurationTooShort,
        DurationTooLong,
        StrengthTooLow,
        MatchedMeanTooLow,
        CoverageTooLow,
        LongestIslandTooShort,
        GapTooLong,
    };

    enum class OnsetRejectReason {
        None,
        BelowThreshold,
        CooldownActive,
        PeakActive,
    };

    ScalarTransientDetector();

    void begin();
    void resetState();
    void resetAcceptedOccurrenceSummary();
    void resetSelectedRejectSummary();
    void update(const AudioSamplePacket& audioSamplePacket, float signalLevel);

    // The thresholds and gates, as one value (step 11e item 5): applied in
    // place, no state reset (a live PARAM SET keeps the occurrence in flight).
    void setConfig(const detection::ScalarTransientConfig& config);
    const detection::ScalarTransientConfig& config() const;

    void buildReport(detection::DetectorReport& out, unsigned long nowMs) const;
    const detection::DetectorReport& latestReport() const;
    uint32_t reportGeneration() const;
    // Bumped only when the selected reject changes. A weaker reject refreezes
    // the report (rejected_count) without it, so DetectionRuntime emits one
    // RejectedSourceCandidate per new best reject (decision 2026-10-13).
    uint32_t selectedRejectGeneration() const;
    bool popOccurrence(detection::Occurrence& out);
    bool hasPendingOccurrence() const;

private:
    // Thresholds and gates (ScalarTransientConfig.h documents the fields).
    detection::ScalarTransientConfig _config = {};

    // Live gate state: this update's onset / transient flags and the onset cooldown.
    struct GateState {
        bool onsetDetected = false;
        float onsetStrength = 0.0f;
        uint64_t lastOnsetUs = 0;
        bool transientDetected = false;
        float transientStrength = 0.0f;
        unsigned long transientDurationMs = 0;
        // Set when a candidate was closed for running past max duration while the
        // signal was still above release; no new candidate opens until it drops
        // below release, so one continuous tone gives one reject (step 8b item 3).
        bool awaitReleaseBeforeOnset = false;
    };
    GateState _gate = {};

    // The open peak: from onset until the release debounce (or max duration) closes it.
    struct PeakState {
        bool active = false;
        uint64_t startedUs = 0;
        uint64_t strengthObservedUs = 0;
        uint64_t releaseCandidateStartedUs = 0;
        uint64_t releaseObservedUs = 0;
        float strength = 0.0f;
    };
    PeakState _peak = {};

    // What the open peak measured so far (strength sums, coverage, islands and gaps); the summaries copy it.
    struct CandidateFacts {
        float peak = 0.0f;
        double strengthSum = 0.0;
        uint32_t strengthCount = 0;
        double matchedStrengthSum = 0.0;
        uint32_t matchedStrengthCount = 0;
        float sumSquares = 0.0f;
        uint64_t coverageAboveAttackUs = 0;
        uint64_t coverageAboveReleaseUs = 0;
        uint64_t sustainedUs = 0;
        unsigned int islandCount = 0;
        unsigned int gapCount = 0;
        unsigned long islandMaxMs = 0;
        unsigned long gapMaxMs = 0;
        bool wasAboveRelease = false;
        uint64_t currentIslandStartUs = 0;
        uint64_t currentGapStartUs = 0;
        uint64_t lastUpdateUs = 0;

        float mean() const {
            return strengthCount > 0 ? static_cast<float>(strengthSum / static_cast<double>(strengthCount)) : 0.0f;
        }
        float matchedMean() const {
            return matchedStrengthCount > 0
                ? static_cast<float>(matchedStrengthSum / static_cast<double>(matchedStrengthCount))
                : 0.0f;
        }
        float rms() const { return strengthCount > 0 ? sqrtf(sumSquares / static_cast<float>(strengthCount)) : 0.0f; }
    };
    CandidateFacts _facts = {};

    // Candidate lifecycle: the occurrence followed sample by sample from onset, emitted when the peak is accepted.
    struct OccurrenceTrack {
        bool active = false;
        uint64_t startSample = 0;
        uint64_t peakSample = 0;
        unsigned long startMs = 0;
        unsigned long peakMs = 0;
        unsigned long holdWindows = 0;
        float onsetStrength = 0.0f;
        float peakStrength = 0.0f;
        float currentStrength = 0.0f;
        bool audioOverflow = false;
    };
    OccurrenceTrack _track = {};
    unsigned long _nextOccurrenceId = 0;
    unsigned long _currentOccurrenceId = 0;

    // Occurrence emission state: the one occurrence waiting for popOccurrence().
    struct Emission {
        bool present = false;
        detection::Occurrence occurrence = {};
    };
    Emission _emission = {};

    // Detector report state: the accepted summary, the selected (best) reject, the frozen report.
    struct AcceptedState {
        bool present = false;
        detection::AcceptedOccurrenceSummary summary = {};
        unsigned long releaseMs = 0;
        unsigned long occurrenceId = 0;
    };
    AcceptedState _accepted = {};
    struct RejectState {
        bool present = false;
        detection::SelectedRejectSummary summary = {};
        unsigned long occurrenceId = 0;
    };
    RejectState _reject = {};
    detection::ScalarDetectorReportDetail _reportDetail = {};
    detection::DetectorReport _latestReport = {};
    uint32_t _reportGeneration = 0;
    uint32_t _selectedRejectGeneration = 0;

    // Reject summary state: why the last onset / transient was refused.
    struct LastReject {
        OnsetRejectReason onset = OnsetRejectReason::None;
        TransientRejectReason transient = TransientRejectReason::None;
        unsigned long durationMs = 0;
        float strength = 0.0f;
    };
    LastReject _lastReject = {};

    // Run-long counts. The report's aggregates are per trial (count minus the base taken at the last per-trial
    // reset), while the counts themselves stay run-long for the success-rate printout (issue #31 R2-14).
    struct Counts {
        unsigned long accepted = 0;
        unsigned long rejected = 0;
        unsigned long tooShort = 0;
        unsigned long tooLong = 0;
        unsigned long strengthTooLow = 0;
        unsigned long trialAcceptedBase = 0;
        unsigned long trialRejectedBase = 0;
        // The rejected count the open track last saw: a reject since then drops the track.
        unsigned long rejectedSeenByTrack = 0;
    };
    Counts _counts = {};

    // Private helpers.
    const char* lastOnsetRejectReasonName() const;
    const char* lastTransientRejectReasonName() const;
    void resetCandidateFacts();
    void updateCandidateFacts(uint64_t nowUs, float strength, bool aboveAttackThreshold, bool aboveReleaseThreshold);
    void finalizeCandidateFacts(uint64_t releaseObservedUs);
    void updateOnsetStage(uint64_t nowUs, float signalMagnitude, bool aboveAttackThreshold, bool onsetCooldownElapsed);
    void updateTransientStage(uint64_t nowUs, float signalMagnitude, bool aboveReleaseThreshold);
    void captureAcceptedOccurrence(uint64_t releaseObservedUs, unsigned long peakDurationUs);
    void captureSelectedReject(uint64_t releaseObservedUs);
    void fillMeasuredSummary(detection::OccurrenceSummary& out) const;
    void updateAcceptedOccurrencePending(const AudioSamplePacket& audioSamplePacket, float signalMagnitude);
    void capturePendingOccurrence(const AudioSamplePacket& audioSamplePacket);
    void resetAcceptedOccurrencePending();
    void freezeReport(unsigned long nowMs);
    void clearFrozenReport();
    // The fields of one summary, without the report clear (resetState clears the report once for both).
    void clearAcceptedSummary();
    void clearSelectedRejectSummary();
    void refreshReportDetail();
};
