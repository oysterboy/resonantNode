#include "FrequencyMatchDetector.h"

#include <cstring>

namespace {

const char* frequencyRejectReasonFromState(const FrequencyMatchDetector& detector) {
    if (detector.pendingAccepted) {
        return "none";
    }
    if (detector.pendingClosed) {
        return detector.noEmitReason[0] != '\0' ? detector.noEmitReason : "unknown";
    }
    return detector.gateReason[0] != '\0' ? detector.gateReason : "unknown";
}

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
    out.accepted.occurrenceId = acceptedOccurrenceId;
    out.frequency.accepted = _acceptedDetail;
    out.thresholds.minDurationMs = pendingMinDurationMs;
    out.thresholds.maxDurationMs = pendingMaxDurationMs;
    out.aggregates.acceptedCount = acceptedCount;
    out.aggregates.rejectedCount = rejectedCount;

    const bool selectedRejectPresent =
        !out.accepted.present &&
        rejectedCount > 0 &&
        (bestOpenMs > 0 || bestPeakMs > 0 || bestCloseMs > 0 || bestDurationMs > 0 || bestPeakScore > 0.0f ||
         bestPeakContrast > 0.0f || (bestRejectReason != nullptr && strcmp(bestRejectReason, "none") != 0));
    if (selectedRejectPresent) {
        out.selectedReject.present = true;
        out.selectedReject.rejectClass = frequencyRejectClassFromReason(bestRejectReason);
        out.selectedReject.detectorReason = bestRejectReason;
        out.selectedReject.occurrenceId = selectedRejectOccurrenceId;
        out.selectedReject.startMs = bestOpenMs;
        out.selectedReject.peakMs = bestPeakMs;
        out.selectedReject.endMs = bestCloseMs;
        out.selectedReject.durationMs = bestDurationMs;
        out.selectedReject.strength = bestPeakScore;
        out.selectedReject.confidence = 0.0f;
        out.selectedReject.peak = bestPeakScore;
        out.selectedReject.mean = bestMean;
        out.selectedReject.rms = bestRms;
        out.selectedReject.coverageAboveAttackMs = bestCoverageAboveAttackMs;
        out.selectedReject.coverageAboveReleaseMs = bestCoverageAboveReleaseMs;
        out.selectedReject.sustainedMs = bestSustainedMs;
        out.selectedReject.islandCount = bestIslandCount;
        out.selectedReject.gapCount = bestGapCount;
        out.selectedReject.islandMaxMs = bestIslandMaxMs;
        out.selectedReject.gapMaxMs = bestGapMaxMs;
        out.frequency.selectedReject.score = bestPeakScore;
        out.frequency.selectedReject.contrast = bestPeakContrast;
    }

    out.frequency.thresholds.scoreThreshold = attackScoreThreshold;
    out.frequency.thresholds.contrastThreshold = attackContrastThreshold;
    out.frequency.aggregates.scoreOkCount = diagnosticsScoreOkCount;
    out.frequency.aggregates.contrastOkCount = diagnosticsContrastOkCount;
    out.frequency.aggregates.bothOkCount = diagnosticsBothOkCount;
    out.frequency.aggregates.matchCount = diagnosticsMatchedCount;
    out.frequency.inspect.rejectReason = frequencyRejectReasonFromState(*this);
    out.frequency.inspect.noEmitReason = noEmitReason;
    out.frequency.inspect.gateReason = gateReason;
    out.frequency.inspect.pendingState = pendingState;
    out.frequency.inspect.readyOk = evidenceOk;
    out.frequency.inspect.gateOpen = attackOk;
    out.frequency.inspect.opened = pendingActive || pendingClosed || pendingAccepted || pendingOpenMs > 0;
    out.frequency.inspect.released = pendingClosed || pendingCloseMs > 0;
    out.frequency.inspect.emitted = pendingAccepted;
    out.frequency.inspect.validRelease = validRelease;
    out.frequency.inspect.emitAllowed = emitAllowed;
    out.frequency.inspect.openMs = pendingOpenMs;
    out.frequency.inspect.peakMs = pendingPeakMs;
    out.frequency.inspect.releaseMs = pendingCloseMs;
    out.frequency.inspect.durationMs = pendingDurationMs;

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
