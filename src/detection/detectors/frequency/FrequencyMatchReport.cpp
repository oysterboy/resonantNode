#include "FrequencyMatchDetector.h"

#include <cstring>

const char* FrequencyMatchDetector::stableReason(const char* text) {
    // Every string the detector copies into _gateReason, _noEmitReason and
    // _pendingState (issue #31).
    static const char* const kKnown[] = {
        "none", "open", "closed", "rejected", "refractory", "duration_too_short",
        "no_frequency_evidence", "freq_score_too_low", "freq_contrast_too_low",
        "freq_score_and_contrast_too_low", "freq_release_score_too_low",
        "freq_release_contrast_too_low", "freq_release_score_and_contrast_too_low",
    };
    if (text == nullptr || text[0] == '\0') {
        return "unknown";
    }
    for (const char* known : kKnown) {
        if (strcmp(text, known) == 0) {
            return known;
        }
    }
    return "unknown";
}

const char* FrequencyMatchDetector::frequencyRejectReasonFromState() const {
    if (_pendingAccepted) {
        return "none";
    }
    if (_pendingClosed) {
        return stableReason(_noEmitReason);
    }
    return stableReason(_gateReason);
}

namespace {

detection::DetectorRejectClass frequencyRejectClassFromReason(const char* reason) {
    if (reason == nullptr || strcmp(reason, "none") == 0) {
        return detection::DetectorRejectClass::None;
    }
    if (strcmp(reason, "duration_too_short") == 0 || strcmp(reason, "duration_too_long") == 0) {
        return detection::DetectorRejectClass::Timing;
    }
    if (strcmp(reason, "refractory") == 0) {
        return detection::DetectorRejectClass::Cooldown;
    }
    if (strstr(reason, "score") != nullptr || strstr(reason, "contrast") != nullptr || strstr(reason, "frequency") != nullptr) {
        return detection::DetectorRejectClass::Threshold;
    }

    return detection::DetectorRejectClass::Unknown;
}

} // namespace

void FrequencyMatchDetector::buildReport(detection::DetectorReport& out, unsigned long nowMs) const {
    out = {};
    out.detectorId = detection::DetectorId::FrequencyMatch;
    out.accepted = _acceptedOccurrence;
    out.accepted.occurrenceId = _acceptedOccurrenceId;
    out.frequency.accepted = _acceptedDetail;
    out.thresholds.minDurationMs = _pendingMinDurationMs;
    out.thresholds.maxDurationMs = _pendingMaxDurationMs;
    out.aggregates.acceptedCount = _acceptedCount;
    out.aggregates.rejectedCount = _rejectedCount;

    const bool selectedRejectPresent =
        !out.accepted.present &&
        _rejectedCount > 0 &&
        (_bestOpenMs > 0 || _bestPeakMs > 0 || _bestCloseMs > 0 || _bestDurationMs > 0 || _bestPeakScore > 0.0f ||
         _bestPeakContrast > 0.0f || (_bestRejectReason != nullptr && strcmp(_bestRejectReason, "none") != 0));
    if (selectedRejectPresent) {
        out.selectedReject.present = true;
        out.selectedReject.rejectClass = frequencyRejectClassFromReason(_bestRejectReason);
        out.selectedReject.detectorReason = _bestRejectReason;
        out.selectedReject.occurrenceId = _selectedRejectOccurrenceId;
        out.selectedReject.startMs = _bestOpenMs;
        out.selectedReject.peakMs = _bestPeakMs;
        out.selectedReject.endMs = _bestCloseMs;
        out.selectedReject.durationMs = _bestDurationMs;
        out.selectedReject.strength = _bestPeakScore;
        out.selectedReject.confidence = 0.0f;
        out.selectedReject.peak = _bestPeakScore;
        out.selectedReject.mean = _bestMean;
        out.selectedReject.rms = _bestRms;
        out.selectedReject.coverageAboveAttackMs = _bestCoverageAboveAttackMs;
        out.selectedReject.coverageAboveReleaseMs = _bestCoverageAboveReleaseMs;
        out.selectedReject.sustainedMs = _bestSustainedMs;
        out.selectedReject.islandCount = _bestIslandCount;
        out.selectedReject.gapCount = _bestGapCount;
        out.selectedReject.islandMaxMs = _bestIslandMaxMs;
        out.selectedReject.gapMaxMs = _bestGapMaxMs;
        out.frequency.selectedReject.score = _bestPeakScore;
        out.frequency.selectedReject.contrast = _bestPeakContrast;
    }

    out.frequency.thresholds.scoreThreshold = _attackScoreThreshold;
    out.frequency.thresholds.contrastThreshold = _attackContrastThreshold;
    out.frequency.aggregates.scoreOkCount = _diagnosticsScoreOkCount;
    out.frequency.aggregates.contrastOkCount = _diagnosticsContrastOkCount;
    out.frequency.aggregates.bothOkCount = _diagnosticsBothOkCount;
    out.frequency.aggregates.matchCount = _diagnosticsMatchedCount;
    out.frequency.inspect.rejectReason = frequencyRejectReasonFromState();
    out.frequency.inspect.noEmitReason = stableReason(_noEmitReason);
    out.frequency.inspect.gateReason = stableReason(_gateReason);
    out.frequency.inspect.pendingState = stableReason(_pendingState);
    out.frequency.inspect.readyOk = _evidenceOk;
    out.frequency.inspect.gateOpen = _attackOk;
    out.frequency.inspect.opened = _pendingActive || _pendingClosed || _pendingAccepted || _pendingOpenMs > 0;
    out.frequency.inspect.released = _pendingClosed || _pendingCloseMs > 0;
    out.frequency.inspect.emitted = _pendingAccepted;
    out.frequency.inspect.validRelease = _validRelease;
    out.frequency.inspect.emitAllowed = _emitAllowed;
    out.frequency.inspect.openMs = _pendingOpenMs;
    out.frequency.inspect.peakMs = _pendingPeakMs;
    out.frequency.inspect.releaseMs = _pendingCloseMs;
    out.frequency.inspect.durationMs = _pendingDurationMs;

    if (out.accepted.present) {
        out.reportStartMs = out.accepted.startMs;
        out.reportEndMs = out.accepted.endMs;
    } else if (out.frequency.inspect.opened) {
        out.reportStartMs = out.frequency.inspect.openMs;
        out.reportEndMs = out.frequency.inspect.released ? out.frequency.inspect.releaseMs : nowMs;
    } else if (out.selectedReject.present) {
        out.reportStartMs = out.selectedReject.startMs;
        out.reportEndMs = out.selectedReject.endMs;
    }
}
