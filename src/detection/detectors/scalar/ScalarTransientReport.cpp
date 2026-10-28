#include "ScalarTransientDetector.h"

#include <Arduino.h>

#include "../../../app/TimingUtils.h"
#include "../../../app/SerialOut.h"

void ScalarTransientDetector::refreshReportDetail() {
    const char* onsetRejectReason = lastOnsetRejectReasonName();
    const char* transientRejectReason = lastTransientRejectReasonName();
    const char* scalarRejectReason = !detection::scalar_transient_detail::detectorReasonIsNone(transientRejectReason)
        ? transientRejectReason
        : onsetRejectReason;
    const bool hasAcceptedSummary = _acceptedOccurrencePresent && _acceptedOccurrence.present;
    const bool hasSelectedRejectSummary = _selectedRejectPresent && _selectedReject.present;
    const bool hasSummary = hasAcceptedSummary || hasSelectedRejectSummary;
    const bool useAcceptedSummary = hasAcceptedSummary || !hasSelectedRejectSummary;
    const float summaryMatchedMeanStrength =
        useAcceptedSummary ? _acceptedOccurrence.matchedMeanStrength : _selectedReject.matchedMeanStrength;
    const char* summaryRejectReason = hasAcceptedSummary
        ? "none"
        : (hasSelectedRejectSummary && _selectedReject.detectorReason != nullptr &&
                   _selectedReject.detectorReason[0] != '\0'
               ? _selectedReject.detectorReason
               : scalarRejectReason);
    _reportDetail.inspect.rejectReason = summaryRejectReason;
    _reportDetail.inspect.noEmitReason = summaryRejectReason;
    _reportDetail.inspect.gateReason = summaryRejectReason;
    _reportDetail.inspect.matchedMeanPassed = !_config.requireMinStrength ||
        (hasSummary ? summaryMatchedMeanStrength >= _config.minMatchedMeanStrength : false);
    _reportDetail.inspect.carrierQualityRequired = _config.requireCarrierQuality;
    _reportDetail.inspect.carrierCoveragePassed = !_config.requireCarrierQuality || !hasSummary ||
        (hasAcceptedSummary ? _acceptedOccurrence.coverageAboveReleaseMs >= _config.minCoverageAboveReleaseMs
                            : _selectedReject.coverageAboveReleaseMs >= _config.minCoverageAboveReleaseMs);
    _reportDetail.inspect.carrierIslandPassed = !_config.requireCarrierQuality || !hasSummary ||
        (hasAcceptedSummary ? _acceptedOccurrence.islandMaxMs >= _config.minLongestIslandMs
                            : _selectedReject.islandMaxMs >= _config.minLongestIslandMs);
    _reportDetail.inspect.carrierGapPassed = !_config.requireCarrierQuality || !hasSummary ||
        (hasAcceptedSummary ? _acceptedOccurrence.gapMaxMs <= _config.maxGapMs
                            : _selectedReject.gapMaxMs <= _config.maxGapMs);
    _reportDetail.inspect.opened = hasAcceptedSummary
        ? true
        : (hasSelectedRejectSummary ? true : (_peakActive || _releaseObservedUs != 0 || _peakStartedUs != 0));
    _reportDetail.inspect.released =
        hasAcceptedSummary ? true : (hasSelectedRejectSummary ? true : (_releaseObservedUs != 0));
    _reportDetail.inspect.validRelease =
        _reportDetail.inspect.released && detection::scalar_transient_detail::detectorReasonIsNone(summaryRejectReason);
    _reportDetail.inspect.emitAllowed = _reportDetail.inspect.validRelease;
    _reportDetail.inspect.openMs = hasAcceptedSummary
        ? _acceptedOccurrence.startMs
        : (hasSelectedRejectSummary ? _selectedReject.startMs : timebase::msFromUs64(_peakStartedUs));
    _reportDetail.inspect.peakMs = hasAcceptedSummary
        ? _acceptedOccurrence.peakMs
        : (hasSelectedRejectSummary ? _selectedReject.peakMs : timebase::msFromUs64(_peakStrengthObservedUs));
    _reportDetail.inspect.releaseMs = hasAcceptedSummary
        ? _acceptedOccurrence.endMs
        : (hasSelectedRejectSummary ? _selectedReject.endMs : timebase::msFromUs64(_releaseObservedUs));
    _reportDetail.inspect.durationMs = _reportDetail.inspect.released &&
            timing::atOrAfter(_reportDetail.inspect.releaseMs, _reportDetail.inspect.openMs)
        ? _reportDetail.inspect.releaseMs - _reportDetail.inspect.openMs
        : 0UL;
    _reportDetail.inspect.peakStrength = hasAcceptedSummary
        ? _acceptedOccurrence.peak
        : (hasSelectedRejectSummary ? _selectedReject.peak : _peakStrength);
    _reportDetail.inspect.rejectReason = summaryRejectReason;
    _reportDetail.thresholds.onsetThreshold = _config.onsetDetectionThreshold;
    _reportDetail.thresholds.releaseThreshold = _config.onsetReleaseThreshold;
    _reportDetail.thresholds.minStrength = _config.minTransientPeakStrength;
    _reportDetail.aggregates.tooShortCount = _transientRejectedDurationTooShortCount;
    _reportDetail.aggregates.tooLongCount = _transientRejectedDurationTooLongCount;
    _reportDetail.aggregates.strengthTooLowCount = _transientRejectedStrengthTooLowCount;
    _reportDetail.aggregates.bestRejectedValue = _selectedRejectPresent ? _selectedReject.strength : 0.0f;
}

void ScalarTransientDetector::buildReport(detection::DetectorReport& out, unsigned long nowMs) const {
    // Keep detector-specific report assembly local to the detector so
    // DetectionRuntime only coordinates report snapshots.
    out = {};
    out.detectorId = detection::DetectorId::ScalarTransient;
    out.accepted = _acceptedOccurrence;
    out.accepted.occurrenceId = _acceptedOccurrenceId;
    out.thresholds.minDurationMs = _config.minTransientDurationMs;
    out.thresholds.maxDurationMs = _config.maxTransientDurationMs;
    out.aggregates.acceptedCount = _peakAcceptedCount - _trialAcceptedCountBase;
    out.aggregates.rejectedCount = _transientRejectedCount - _trialRejectedCountBase;
    out.scalar = _reportDetail;

    const bool selectedRejectPresent = !out.accepted.present && _selectedRejectPresent;
    if (out.accepted.present) {
        out.scalar.inspect.rejectReason = "none";
        out.scalar.inspect.noEmitReason = "none";
        out.scalar.inspect.gateReason = "none";
    }
    if (selectedRejectPresent) {
        out.selectedReject = _selectedReject;
        out.selectedReject.occurrenceId = _selectedRejectOccurrenceId;
        out.scalar.inspect.rejectReason = out.selectedReject.detectorReason;
        out.scalar.inspect.noEmitReason = out.selectedReject.detectorReason;
        out.scalar.inspect.gateReason = out.selectedReject.detectorReason;
    } else {
        out.scalar.selectedReject = {};
    }

    if (out.accepted.present) {
        out.reportStartMs = out.accepted.startMs;
        out.reportEndMs = out.accepted.endMs;
    } else if (out.scalar.inspect.opened) {
        out.reportStartMs = out.scalar.inspect.openMs;
        out.reportEndMs = out.scalar.inspect.released ? out.scalar.inspect.releaseMs : nowMs;
    } else if (out.selectedReject.present) {
        out.reportStartMs = out.selectedReject.startMs;
        out.reportEndMs = out.selectedReject.endMs;
    }
}
