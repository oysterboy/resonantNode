#include "ScalarTransientPrinter.h"

#include <Arduino.h>
#include "../../../app/SerialOut.h"

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
    serialOut().print(prefix);
    serialOut().print(" detail.scalar.accepted.value=");
    serialOut().print(scalar.accepted.value, 1);
    serialOut().print(" detail.scalar.accepted.baseline=");
    serialOut().print(scalar.accepted.baseline, 1);
    serialOut().print(" detail.scalar.accepted.lift=");
    serialOut().print(scalar.accepted.lift, 1);
    serialOut().print(" detail.scalar.accepted.normalized=");
    serialOut().print(scalar.accepted.normalized, 2);
    serialOut().print(" detail.scalar.reject.value=");
    serialOut().print(scalar.selectedReject.value, 1);
    serialOut().print(" detail.scalar.reject.baseline=");
    serialOut().print(scalar.selectedReject.baseline, 1);
    serialOut().print(" detail.scalar.reject.lift=");
    serialOut().print(scalar.selectedReject.lift, 1);
    serialOut().print(" detail.scalar.reject.normalized=");
    serialOut().print(scalar.selectedReject.normalized, 2);
    serialOut().print(" detail.scalar.reject.opened=");
    serialOut().print(scalar.selectedReject.opened ? 1 : 0);
    serialOut().print(" detail.scalar.reject.crossed_onset=");
    serialOut().print(scalar.selectedReject.crossedOnset ? 1 : 0);
    serialOut().print(" detail.scalar.reject.crossed_release=");
    serialOut().print(scalar.selectedReject.crossedRelease ? 1 : 0);
    serialOut().print(" detail.scalar.threshold.onset=");
    serialOut().print(scalar.thresholds.onsetThreshold, 1);
    serialOut().print(" detail.scalar.threshold.release=");
    serialOut().print(scalar.thresholds.releaseThreshold, 1);
    serialOut().print(" detail.scalar.threshold.min_strength=");
    serialOut().print(scalar.thresholds.minStrength, 1);
    serialOut().print(" detail.scalar.aggregate.too_short=");
    serialOut().print(scalar.aggregates.tooShortCount);
    serialOut().print(" detail.scalar.aggregate.too_long=");
    serialOut().print(scalar.aggregates.tooLongCount);
    serialOut().print(" detail.scalar.aggregate.strength_too_low=");
    serialOut().print(scalar.aggregates.strengthTooLowCount);
    serialOut().print(" detail.scalar.aggregate.max_rejected_lift=");
    serialOut().print(scalar.aggregates.maxRejectedLift, 1);
    serialOut().print(" detail.scalar.aggregate.best_rejected_value=");
    serialOut().print(scalar.aggregates.bestRejectedValue, 1);
    serialOut().print(" detail.scalar.inspect.reject_reason=");
    serialOut().print(selectedInspectReason);
    serialOut().print(" detail.scalar.inspect.no_emit_reason=");
    serialOut().print(selectedInspectReason);
    serialOut().print(" detail.scalar.inspect.gate_reason=");
    serialOut().print(selectedInspectReason);
    serialOut().print(" detail.scalar.inspect.min_strength_pass=");
    serialOut().print(scalar.inspect.matchedMeanPassed ? 1 : 0);
    serialOut().print(" detail.scalar.inspect.carrier_quality_required=");
    serialOut().print(scalar.inspect.carrierQualityRequired ? 1 : 0);
    serialOut().print(" detail.scalar.inspect.carrier_coverage_passed=");
    serialOut().print(scalar.inspect.carrierCoveragePassed ? 1 : 0);
    serialOut().print(" detail.scalar.inspect.carrier_island_passed=");
    serialOut().print(scalar.inspect.carrierIslandPassed ? 1 : 0);
    serialOut().print(" detail.scalar.inspect.carrier_gap_passed=");
    serialOut().print(scalar.inspect.carrierGapPassed ? 1 : 0);
    serialOut().print(" detail.scalar.inspect.opened=");
    serialOut().print(scalar.inspect.opened ? 1 : 0);
    serialOut().print(" detail.scalar.inspect.released=");
    serialOut().print(scalar.inspect.released ? 1 : 0);
    serialOut().print(" detail.scalar.inspect.valid_release=");
    serialOut().print(scalar.inspect.validRelease ? 1 : 0);
    serialOut().print(" detail.scalar.inspect.emit_allowed=");
    serialOut().print(scalar.inspect.emitAllowed ? 1 : 0);
    serialOut().print(" detail.scalar.inspect.open_ms=");
    serialOut().print(scalar.inspect.openMs);
    serialOut().print(" detail.scalar.inspect.peak_ms=");
    serialOut().print(scalar.inspect.peakMs);
    serialOut().print(" detail.scalar.inspect.release_ms=");
    serialOut().print(scalar.inspect.releaseMs);
    serialOut().print(" detail.scalar.inspect.duration_ms=");
    serialOut().print(scalar.inspect.durationMs);
    serialOut().print(" detail.scalar.inspect.peak_strength=");
    serialOut().println(scalar.inspect.peakStrength, 1);
}

} // namespace detection::scalar
