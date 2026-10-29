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
    const bool hasAcceptedSummary = _accepted.present && _accepted.summary.present;
    const bool hasSelectedRejectSummary = _reject.present && _reject.summary.present;
    const bool hasSummary = hasAcceptedSummary || hasSelectedRejectSummary;
    const bool useAcceptedSummary = hasAcceptedSummary || !hasSelectedRejectSummary;
    const float summaryMatchedMeanStrength =
        useAcceptedSummary ? _accepted.summary.matchedMeanStrength : _reject.summary.matchedMeanStrength;
    const char* summaryRejectReason = hasAcceptedSummary
        ? "none"
        : (hasSelectedRejectSummary && _reject.summary.detectorReason != nullptr &&
                   _reject.summary.detectorReason[0] != '\0'
               ? _reject.summary.detectorReason
               : scalarRejectReason);
    _reportDetail.inspect.rejectReason = summaryRejectReason;
    _reportDetail.inspect.noEmitReason = summaryRejectReason;
    _reportDetail.inspect.gateReason = summaryRejectReason;
    _reportDetail.inspect.matchedMeanPassed = !_config.requireMinStrength ||
        (hasSummary ? summaryMatchedMeanStrength >= _config.minMatchedMeanStrength : false);
    _reportDetail.inspect.carrierQualityRequired = _config.requireCarrierQuality;
    _reportDetail.inspect.carrierCoveragePassed = !_config.requireCarrierQuality || !hasSummary ||
        (hasAcceptedSummary ? _accepted.summary.coverageAboveReleaseMs >= _config.minCoverageAboveReleaseMs
                            : _reject.summary.coverageAboveReleaseMs >= _config.minCoverageAboveReleaseMs);
    _reportDetail.inspect.carrierIslandPassed = !_config.requireCarrierQuality || !hasSummary ||
        (hasAcceptedSummary ? _accepted.summary.islandMaxMs >= _config.minLongestIslandMs
                            : _reject.summary.islandMaxMs >= _config.minLongestIslandMs);
    _reportDetail.inspect.carrierGapPassed = !_config.requireCarrierQuality || !hasSummary ||
        (hasAcceptedSummary ? _accepted.summary.gapMaxMs <= _config.maxGapMs
                            : _reject.summary.gapMaxMs <= _config.maxGapMs);
    _reportDetail.inspect.opened = hasAcceptedSummary
        ? true
        : (hasSelectedRejectSummary ? true : (_peak.active || _peak.releaseObservedUs != 0 || _peak.startedUs != 0));
    _reportDetail.inspect.released =
        hasAcceptedSummary ? true : (hasSelectedRejectSummary ? true : (_peak.releaseObservedUs != 0));
    _reportDetail.inspect.validRelease =
        _reportDetail.inspect.released && detection::scalar_transient_detail::detectorReasonIsNone(summaryRejectReason);
    _reportDetail.inspect.emitAllowed = _reportDetail.inspect.validRelease;
    _reportDetail.inspect.openMs = hasAcceptedSummary
        ? _accepted.summary.startMs
        : (hasSelectedRejectSummary ? _reject.summary.startMs : timebase::msFromUs64(_peak.startedUs));
    _reportDetail.inspect.peakMs = hasAcceptedSummary
        ? _accepted.summary.peakMs
        : (hasSelectedRejectSummary ? _reject.summary.peakMs : timebase::msFromUs64(_peak.strengthObservedUs));
    _reportDetail.inspect.releaseMs = hasAcceptedSummary
        ? _accepted.summary.endMs
        : (hasSelectedRejectSummary ? _reject.summary.endMs : timebase::msFromUs64(_peak.releaseObservedUs));
    _reportDetail.inspect.durationMs = _reportDetail.inspect.released &&
            timing::atOrAfter(_reportDetail.inspect.releaseMs, _reportDetail.inspect.openMs)
        ? _reportDetail.inspect.releaseMs - _reportDetail.inspect.openMs
        : 0UL;
    _reportDetail.inspect.peakStrength = hasAcceptedSummary
        ? _accepted.summary.peak
        : (hasSelectedRejectSummary ? _reject.summary.peak : _peak.strength);
    _reportDetail.inspect.rejectReason = summaryRejectReason;
    _reportDetail.thresholds.onsetThreshold = _config.onsetDetectionThreshold;
    _reportDetail.thresholds.releaseThreshold = _config.onsetReleaseThreshold;
    _reportDetail.thresholds.minStrength = _config.minTransientPeakStrength;
    _reportDetail.aggregates.tooShortCount = _counts.tooShort;
    _reportDetail.aggregates.tooLongCount = _counts.tooLong;
    _reportDetail.aggregates.strengthTooLowCount = _counts.strengthTooLow;
    _reportDetail.aggregates.bestRejectedValue = _reject.present ? _reject.summary.strength : 0.0f;
}

void ScalarTransientDetector::buildReport(detection::DetectorReport& out, unsigned long nowMs) const {
    // Keep detector-specific report assembly local to the detector so
    // DetectionRuntime only coordinates report snapshots.
    out = {};
    out.detectorId = detection::DetectorId::ScalarTransient;
    out.accepted = _accepted.summary;
    out.accepted.occurrenceId = _accepted.occurrenceId;
    out.thresholds.minDurationMs = _config.minTransientDurationMs;
    out.thresholds.maxDurationMs = _config.maxTransientDurationMs;
    out.aggregates.acceptedCount = _counts.accepted - _counts.trialAcceptedBase;
    out.aggregates.rejectedCount = _counts.rejected - _counts.trialRejectedBase;
    out.scalar = _reportDetail;

    const bool selectedRejectPresent = !out.accepted.present && _reject.present;
    if (out.accepted.present) {
        out.scalar.inspect.rejectReason = "none";
        out.scalar.inspect.noEmitReason = "none";
        out.scalar.inspect.gateReason = "none";
    }
    if (selectedRejectPresent) {
        out.selectedReject = _reject.summary;
        out.selectedReject.occurrenceId = _reject.occurrenceId;
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
