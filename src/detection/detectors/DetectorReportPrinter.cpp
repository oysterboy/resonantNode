#include "DetectorReportPrinter.h"

#include <Arduino.h>

#include "DetectorNames.h"
#include "frequency/FrequencyMatchPrinter.h"
#include "scalar/ScalarTransientPrinter.h"

// Shared detector report rendering. Dispatches to detector-owned detail printers.
namespace detection {

void printDetectorReportGenericLine(const char* prefix, unsigned long trial, const DetectorReport* report) {
    static const DetectorReport kEmptyDetectorReport = {};
    const auto& detectorReport = report != nullptr ? *report : kEmptyDetectorReport;
    const auto& accepted = detectorReport.accepted;
    const auto& selectedReject = detectorReport.selectedReject;

    Serial.print(prefix);
    Serial.print(" trial=");
    Serial.print(trial);
    Serial.print(" detector=");
    Serial.print(detectorIdName(detectorReport.detectorId));
    Serial.print(" window.start_ms=");
    Serial.print(detectorReport.reportStartMs);
    Serial.print(" window.end_ms=");
    Serial.print(detectorReport.reportEndMs);
    Serial.print(" accepted.present=");
    Serial.print(accepted.present ? 1 : 0);
    Serial.print(" accepted.start_ms=");
    Serial.print(accepted.startMs);
    Serial.print(" accepted.peak_ms=");
    Serial.print(accepted.peakMs);
    Serial.print(" accepted.end_ms=");
    Serial.print(accepted.endMs);
    Serial.print(" accepted.duration_ms=");
    Serial.print(accepted.durationMs);
    Serial.print(" accepted.strength=");
    Serial.print(accepted.strength, 1);
    Serial.print(" accepted.confidence=");
    Serial.print(accepted.confidence, 2);
    Serial.print(" accepted.peak=");
    Serial.print(accepted.peak, 1);
    Serial.print(" accepted.mean=");
    Serial.print(accepted.mean, 1);
    Serial.print(" accepted.rms=");
    Serial.print(accepted.rms, 1);
    Serial.print(" accepted.coverage_attack_ms=");
    Serial.print(accepted.coverageAboveAttackMs);
    Serial.print(" accepted.coverage_release_ms=");
    Serial.print(accepted.coverageAboveReleaseMs);
    Serial.print(" accepted.sustained_ms=");
    Serial.print(accepted.sustainedMs);
    Serial.print(" accepted.island_count=");
    Serial.print(accepted.islandCount);
    Serial.print(" accepted.gap_count=");
    Serial.print(accepted.gapCount);
    Serial.print(" accepted.island_max_ms=");
    Serial.print(accepted.islandMaxMs);
    Serial.print(" accepted.gap_max_ms=");
    Serial.print(accepted.gapMaxMs);
    Serial.print(" reject.present=");
    Serial.print(selectedReject.present ? 1 : 0);
    Serial.print(" reject.class=");
    Serial.print(detectorRejectClassName(selectedReject.rejectClass));
    Serial.print(" reject.detector_reason=");
    Serial.print(selectedReject.detectorReason != nullptr ? selectedReject.detectorReason : "none");
    Serial.print(" reject.start_ms=");
    Serial.print(selectedReject.startMs);
    Serial.print(" reject.peak_ms=");
    Serial.print(selectedReject.peakMs);
    Serial.print(" reject.end_ms=");
    Serial.print(selectedReject.endMs);
    Serial.print(" reject.duration_ms=");
    Serial.print(selectedReject.durationMs);
    Serial.print(" reject.strength=");
    Serial.print(selectedReject.strength, 1);
    Serial.print(" reject.confidence=");
    Serial.print(selectedReject.confidence, 2);
    Serial.print(" reject.peak=");
    Serial.print(selectedReject.peak, 1);
    Serial.print(" reject.mean=");
    Serial.print(selectedReject.mean, 1);
    Serial.print(" reject.rms=");
    Serial.print(selectedReject.rms, 1);
    const bool useAcceptedCandidate = accepted.present || !selectedReject.present;
    const float candidateMeanStrength = useAcceptedCandidate ? accepted.meanStrength : selectedReject.meanStrength;
    const float candidateMatchedMeanStrength = useAcceptedCandidate
        ? accepted.matchedMeanStrength
        : selectedReject.matchedMeanStrength;
    const unsigned int candidateStrengthCount = useAcceptedCandidate
        ? accepted.strengthCount
        : selectedReject.strengthCount;
    const unsigned int candidateMatchedStrengthCount = useAcceptedCandidate
        ? accepted.matchedStrengthCount
        : selectedReject.matchedStrengthCount;
    Serial.print(" candidate.mean_strength=");
    Serial.print(candidateMeanStrength, 1);
    Serial.print(" candidate.min_strength=");
    Serial.print(candidateMatchedMeanStrength, 1);
    Serial.print(" candidate.strength_count=");
    Serial.print(candidateStrengthCount);
    Serial.print(" candidate.matched_strength_count=");
    Serial.print(candidateMatchedStrengthCount);
    Serial.print(" reject.coverage_attack_ms=");
    Serial.print(selectedReject.coverageAboveAttackMs);
    Serial.print(" reject.coverage_release_ms=");
    Serial.print(selectedReject.coverageAboveReleaseMs);
    Serial.print(" reject.sustained_ms=");
    Serial.print(selectedReject.sustainedMs);
    Serial.print(" reject.island_count=");
    Serial.print(selectedReject.islandCount);
    Serial.print(" reject.gap_count=");
    Serial.print(selectedReject.gapCount);
    Serial.print(" reject.island_max_ms=");
    Serial.print(selectedReject.islandMaxMs);
    Serial.print(" reject.gap_max_ms=");
    Serial.print(selectedReject.gapMaxMs);
    Serial.print(" carrier.quality_pass=");
    if (detectorReport.detectorId == DetectorId::ScalarTransient) {
        const bool carrierQualityPass = detectorReport.scalar.inspect.carrierQualityRequired
            ? (detectorReport.scalar.inspect.carrierCoveragePassed &&
               detectorReport.scalar.inspect.carrierIslandPassed &&
               detectorReport.scalar.inspect.carrierGapPassed)
            : true;
        Serial.print(carrierQualityPass ? 1 : 0);
    } else if (accepted.present) {
        Serial.print(1);
    } else {
        Serial.print(0);
    }
    Serial.print(" candidate.min_strength_pass=");
    Serial.print(detectorReport.detectorId == DetectorId::ScalarTransient &&
        detectorReport.scalar.inspect.matchedMeanPassed ? 1 : 0);
    Serial.print(" threshold.min_duration_ms=");
    Serial.print(detectorReport.thresholds.minDurationMs);
    Serial.print(" threshold.max_duration_ms=");
    Serial.print(detectorReport.thresholds.maxDurationMs);
    Serial.print(" aggregate.accepted_count=");
    Serial.print(detectorReport.aggregates.acceptedCount);
    Serial.print(" aggregate.rejected_count=");
    Serial.println(detectorReport.aggregates.rejectedCount);
}

void printDetectorDetailLine(const char* prefix, const DetectorReport* report) {
    if (report == nullptr) {
        return;
    }

    switch (report->detectorId) {
        case DetectorId::ScalarTransient:
            scalar::printScalarTransientDetailLine(prefix, report);
            break;
        case DetectorId::FrequencyMatch:
            frequency::printFrequencyMatchDetailLine(prefix, report);
            break;
        case DetectorId::Unknown:
        default:
            break;
    }
}

} // namespace detection
