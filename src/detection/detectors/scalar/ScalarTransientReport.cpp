#include "ScalarTransientDetector.h"

#include <Arduino.h>
#include <string.h>

namespace {

bool detectorReasonIsNone(const char* reason) {
    return reason == nullptr || strcmp(reason, "none") == 0;
}

} // namespace

void ScalarTransientDetector::refreshReportDetail() {
    const char* onsetRejectReason = lastOnsetRejectReasonName();
    const char* transientRejectReason = lastTransientRejectReasonName();
    const char* scalarRejectReason = !detectorReasonIsNone(transientRejectReason)
        ? transientRejectReason
        : onsetRejectReason;

    _reportDetail.inspect.rejectReason = scalarRejectReason;
    _reportDetail.inspect.noEmitReason = scalarRejectReason;
    _reportDetail.inspect.gateReason = scalarRejectReason;
    _reportDetail.inspect.opened = _peakActive || _releaseObservedUs != 0 || _peakStartedUs != 0;
    _reportDetail.inspect.released = _releaseObservedUs != 0;
    _reportDetail.inspect.validRelease = _reportDetail.inspect.released && detectorReasonIsNone(scalarRejectReason);
    _reportDetail.inspect.emitAllowed = _reportDetail.inspect.validRelease;
    _reportDetail.inspect.openMs = _peakStartedUs / 1000UL;
    _reportDetail.inspect.peakMs = _peakStrengthObservedUs / 1000UL;
    _reportDetail.inspect.releaseMs = _releaseObservedUs / 1000UL;
    _reportDetail.inspect.durationMs = _reportDetail.inspect.released && _reportDetail.inspect.releaseMs >= _reportDetail.inspect.openMs
        ? _reportDetail.inspect.releaseMs - _reportDetail.inspect.openMs
        : 0UL;
    _reportDetail.inspect.peakStrength = _peakStrength;
    _reportDetail.inspect.rejectReason = scalarRejectReason;
    _reportDetail.thresholds.onsetThreshold = _onsetDetectionThreshold;
    _reportDetail.thresholds.releaseThreshold = _onsetReleaseThreshold;
    _reportDetail.thresholds.minStrength = _minTransientPeakStrength;
    _reportDetail.aggregates.tooShortCount = _transientRejectedDurationTooShortCount;
    _reportDetail.aggregates.tooLongCount = _transientRejectedDurationTooLongCount;
    _reportDetail.aggregates.strengthTooLowCount = _transientRejectedStrengthTooLowCount;
    _reportDetail.aggregates.maxRejectedLift = 0.0f;
    _reportDetail.aggregates.bestRejectedValue = _selectedRejectPresent ? _selectedReject.strength : 0.0f;
}

void ScalarTransientDetector::printTransientStatsIfDue(unsigned long nowUs) {
    if (!_diagnosticsEnabled || !AUDIO_VERBOSE_DEBUG) {
        return;
    }

    if (_diagnosticsLabel == nullptr) {
        _diagnosticsLabel = "EVT";
    }

    if (_lastStatsPrintUs == 0 || nowUs - _lastStatsPrintUs >= _statsPrintIntervalMs * 1000UL) {
        const unsigned long elapsedMs = (nowUs - _statsStartUs) / 1000UL;
        const unsigned long expectedCount = (elapsedMs + (_expectedTransientPeriodMs / 2)) / _expectedTransientPeriodMs;
        const unsigned long successRate = expectedCount > 0 ? ((_peakAcceptedCount * 100UL) / expectedCount) : 0;

        Serial.print(_diagnosticsLabel);
        Serial.print(" transient success t=");
        Serial.print(nowUs / 1000UL);
        Serial.print(" accepted=");
        Serial.print(_peakAcceptedCount);
        Serial.print(" expected=");
        Serial.print(expectedCount);
        Serial.print(" success_rate=");
        Serial.print(successRate);
        Serial.print("%");
        Serial.print(" onset_rejected=");
        Serial.print(_onsetRejectedCount);
        Serial.print(" transient_rejected=");
        Serial.print(_transientRejectedCount);
        Serial.print(" duration_short=");
        Serial.print(_transientRejectedDurationTooShortCount);
        Serial.print(" duration_long=");
        Serial.print(_transientRejectedDurationTooLongCount);
        Serial.print(" strength_low=");
        Serial.print(_transientRejectedStrengthTooLowCount);
        Serial.print(" peak_strength=");
        Serial.print(_acceptedOccurrence.peak, 1);
        Serial.print(" best_rejected_strength=");
        Serial.print(_selectedReject.strength, 1);
        Serial.print(" best_rejected_reason=");
        Serial.println(_selectedReject.detectorReason != nullptr ? _selectedReject.detectorReason : "none");
        _lastStatsPrintUs = nowUs;
    }
}

void ScalarTransientDetector::buildReport(detection::DetectorReport& out, unsigned long nowMs) const {
    // Keep detector-specific report assembly local to the detector so
    // DetectionRuntime only coordinates report snapshots.
    out = {};
    out.detectorId = detection::DetectorId::ScalarTransient;
    out.accepted = _acceptedOccurrence;
    out.thresholds.minDurationMs = _minTransientDurationMs;
    out.thresholds.maxDurationMs = _maxTransientDurationMs;
    out.aggregates.acceptedCount = _peakAcceptedCount;
    out.aggregates.rejectedCount = _transientRejectedCount;
    out.scalar = _reportDetail;

    const bool selectedRejectPresent = !out.accepted.present && _selectedRejectPresent;
    if (selectedRejectPresent) {
        out.selectedReject = _selectedReject;
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
