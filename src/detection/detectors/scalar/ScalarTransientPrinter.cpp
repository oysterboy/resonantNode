#include "ScalarTransientPrinter.h"

#include <Arduino.h>

// Scalar-transient detector-specific rendering implementation.
namespace detection::scalar {

void printScalarTransientDetailLine(const char* prefix, const detection::DetectorReport* report) {
    if (report == nullptr || report->detectorId != detection::DetectorId::ScalarTransient) {
        return;
    }

    const auto& scalar = report->scalar;
    const bool acceptedSelected = report->accepted.present;
    const bool rejectSelected = !acceptedSelected && report->selectedReject.present;
    const char* selectedInspectReason = rejectSelected &&
        report->selectedReject.detectorReason != nullptr &&
        report->selectedReject.detectorReason[0] != '\0'
            ? report->selectedReject.detectorReason
            : "none";
    Serial.print(prefix);
    Serial.print(" detail.scalar.accepted.value=");
    Serial.print(scalar.accepted.value, 1);
    Serial.print(" detail.scalar.accepted.baseline=");
    Serial.print(scalar.accepted.baseline, 1);
    Serial.print(" detail.scalar.accepted.lift=");
    Serial.print(scalar.accepted.lift, 1);
    Serial.print(" detail.scalar.accepted.normalized=");
    Serial.print(scalar.accepted.normalized, 2);
    Serial.print(" detail.scalar.reject.value=");
    Serial.print(scalar.selectedReject.value, 1);
    Serial.print(" detail.scalar.reject.baseline=");
    Serial.print(scalar.selectedReject.baseline, 1);
    Serial.print(" detail.scalar.reject.lift=");
    Serial.print(scalar.selectedReject.lift, 1);
    Serial.print(" detail.scalar.reject.normalized=");
    Serial.print(scalar.selectedReject.normalized, 2);
    Serial.print(" detail.scalar.reject.opened=");
    Serial.print(scalar.selectedReject.opened ? 1 : 0);
    Serial.print(" detail.scalar.reject.crossed_onset=");
    Serial.print(scalar.selectedReject.crossedOnset ? 1 : 0);
    Serial.print(" detail.scalar.reject.crossed_release=");
    Serial.print(scalar.selectedReject.crossedRelease ? 1 : 0);
    Serial.print(" detail.scalar.threshold.onset=");
    Serial.print(scalar.thresholds.onsetThreshold, 1);
    Serial.print(" detail.scalar.threshold.release=");
    Serial.print(scalar.thresholds.releaseThreshold, 1);
    Serial.print(" detail.scalar.threshold.min_strength=");
    Serial.print(scalar.thresholds.minStrength, 1);
    Serial.print(" detail.scalar.aggregate.too_short=");
    Serial.print(scalar.aggregates.tooShortCount);
    Serial.print(" detail.scalar.aggregate.too_long=");
    Serial.print(scalar.aggregates.tooLongCount);
    Serial.print(" detail.scalar.aggregate.strength_too_low=");
    Serial.print(scalar.aggregates.strengthTooLowCount);
    Serial.print(" detail.scalar.aggregate.max_rejected_lift=");
    Serial.print(scalar.aggregates.maxRejectedLift, 1);
    Serial.print(" detail.scalar.aggregate.best_rejected_value=");
    Serial.print(scalar.aggregates.bestRejectedValue, 1);
    Serial.print(" detail.scalar.inspect.reject_reason=");
    Serial.print(selectedInspectReason);
    Serial.print(" detail.scalar.inspect.no_emit_reason=");
    Serial.print(selectedInspectReason);
    Serial.print(" detail.scalar.inspect.gate_reason=");
    Serial.print(selectedInspectReason);
    Serial.print(" detail.scalar.inspect.min_strength_pass=");
    Serial.print(scalar.inspect.matchedMeanPassed ? 1 : 0);
    Serial.print(" detail.scalar.inspect.carrier_quality_required=");
    Serial.print(scalar.inspect.carrierQualityRequired ? 1 : 0);
    Serial.print(" detail.scalar.inspect.carrier_coverage_passed=");
    Serial.print(scalar.inspect.carrierCoveragePassed ? 1 : 0);
    Serial.print(" detail.scalar.inspect.carrier_island_passed=");
    Serial.print(scalar.inspect.carrierIslandPassed ? 1 : 0);
    Serial.print(" detail.scalar.inspect.carrier_gap_passed=");
    Serial.print(scalar.inspect.carrierGapPassed ? 1 : 0);
    Serial.print(" detail.scalar.inspect.opened=");
    Serial.print(scalar.inspect.opened ? 1 : 0);
    Serial.print(" detail.scalar.inspect.released=");
    Serial.print(scalar.inspect.released ? 1 : 0);
    Serial.print(" detail.scalar.inspect.valid_release=");
    Serial.print(scalar.inspect.validRelease ? 1 : 0);
    Serial.print(" detail.scalar.inspect.emit_allowed=");
    Serial.print(scalar.inspect.emitAllowed ? 1 : 0);
    Serial.print(" detail.scalar.inspect.open_ms=");
    Serial.print(scalar.inspect.openMs);
    Serial.print(" detail.scalar.inspect.peak_ms=");
    Serial.print(scalar.inspect.peakMs);
    Serial.print(" detail.scalar.inspect.release_ms=");
    Serial.print(scalar.inspect.releaseMs);
    Serial.print(" detail.scalar.inspect.duration_ms=");
    Serial.print(scalar.inspect.durationMs);
    Serial.print(" detail.scalar.inspect.peak_strength=");
    Serial.println(scalar.inspect.peakStrength, 1);
}

} // namespace detection::scalar
