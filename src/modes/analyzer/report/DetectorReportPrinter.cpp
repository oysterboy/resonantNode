#include "DetectorReportPrinter.h"

#include <Arduino.h>

#include "../../../detection/detectors/DetectorNames.h"
#include "ScalarTransientPrinter.h"
#include "../../../app/SerialOut.h"

#include "../../../app/PrintKv.h"
// Shared detector report rendering. Dispatches to detector-owned detail printers.
namespace detection {

void printDetectorReportGenericLine(const char* prefix, unsigned long trial, const DetectorReport* report) {
    static const DetectorReport kEmptyDetectorReport = {};
    const auto& detectorReport = report != nullptr ? *report : kEmptyDetectorReport;
    const auto& accepted = detectorReport.accepted;
    const auto& selectedReject = detectorReport.selectedReject;

    serialOut().print(prefix);
    printKv("trial", trial);
    printKv("detector", detectorIdName(detectorReport.detectorId));
    printKv("source.selection", detectorReport.sourceSelection != nullptr ? detectorReport.sourceSelection : "none");
    printKv("source.occurrence_id", detectorReport.sourceOccurrenceId);
    printKv("source.candidate_id", detectorReport.sourceCandidateId);
    printKv("source.report_matched", detectorReport.sourceReportMatched ? 1 : 0);
    printKv("window.start_ms", detectorReport.reportStartMs);
    printKv("window.end_ms", detectorReport.reportEndMs);
    printKv("accepted.present", accepted.present ? 1 : 0);
    printKv("accepted.start_ms", accepted.startMs);
    printKv("accepted.peak_ms", accepted.peakMs);
    printKv("accepted.end_ms", accepted.endMs);
    printKv("accepted.duration_ms", accepted.durationMs);
    printKv("accepted.strength", accepted.strength, 1);
    printKv("accepted.peak", accepted.peak, 1);
    printKv("accepted.mean", accepted.mean, 1);
    printKv("accepted.rms", accepted.rms, 1);
    printKv("accepted.coverage_attack_ms", accepted.coverageAboveAttackMs);
    printKv("accepted.coverage_release_ms", accepted.coverageAboveReleaseMs);
    printKv("accepted.sustained_ms", accepted.sustainedMs);
    printKv("accepted.island_count", accepted.islandCount);
    printKv("accepted.gap_count", accepted.gapCount);
    printKv("accepted.island_max_ms", accepted.islandMaxMs);
    printKv("accepted.gap_max_ms", accepted.gapMaxMs);
    printKv("reject.present", selectedReject.present ? 1 : 0);
    printKv("reject.class", detectorRejectClassName(selectedReject.rejectClass));
    printKv("reject.detector_reason",
            selectedReject.detectorReason != nullptr ? selectedReject.detectorReason : "none");
    printKv("reject.start_ms", selectedReject.startMs);
    printKv("reject.peak_ms", selectedReject.peakMs);
    printKv("reject.end_ms", selectedReject.endMs);
    printKv("reject.duration_ms", selectedReject.durationMs);
    printKv("reject.strength", selectedReject.strength, 1);
    printKv("reject.peak", selectedReject.peak, 1);
    printKv("reject.mean", selectedReject.mean, 1);
    printKv("reject.rms", selectedReject.rms, 1);
    const bool useAcceptedCandidate = accepted.present || !selectedReject.present;
    const float candidateMeanStrength = useAcceptedCandidate ? accepted.meanStrength : selectedReject.meanStrength;
    const float candidateMatchedMeanStrength =
        useAcceptedCandidate ? accepted.matchedMeanStrength : selectedReject.matchedMeanStrength;
    const unsigned int candidateStrengthCount =
        useAcceptedCandidate ? accepted.strengthCount : selectedReject.strengthCount;
    const unsigned int candidateMatchedStrengthCount =
        useAcceptedCandidate ? accepted.matchedStrengthCount : selectedReject.matchedStrengthCount;
    printKv("candidate.mean_strength", candidateMeanStrength, 1);
    printKv("candidate.min_strength", candidateMatchedMeanStrength, 1);
    printKv("candidate.strength_count", candidateStrengthCount);
    printKv("candidate.matched_strength_count", candidateMatchedStrengthCount);
    printKv("reject.coverage_attack_ms", selectedReject.coverageAboveAttackMs);
    printKv("reject.coverage_release_ms", selectedReject.coverageAboveReleaseMs);
    printKv("reject.sustained_ms", selectedReject.sustainedMs);
    printKv("reject.island_count", selectedReject.islandCount);
    printKv("reject.gap_count", selectedReject.gapCount);
    printKv("reject.island_max_ms", selectedReject.islandMaxMs);
    printKv("reject.gap_max_ms", selectedReject.gapMaxMs);
    serialOut().print(" carrier.quality_pass=");
    if (detectorReport.detectorId == DetectorId::ScalarTransient) {
        const bool carrierQualityPass = detectorReport.scalar.inspect.carrierQualityRequired
            ? (detectorReport.scalar.inspect.carrierCoveragePassed &&
               detectorReport.scalar.inspect.carrierIslandPassed && detectorReport.scalar.inspect.carrierGapPassed)
            : true;
        serialOut().print(carrierQualityPass ? 1 : 0);
    } else if (accepted.present) {
        serialOut().print(1);
    } else {
        serialOut().print(0);
    }
    printKv("candidate.min_strength_pass",
            detectorReport.detectorId == DetectorId::ScalarTransient && detectorReport.scalar.inspect.matchedMeanPassed
                ? 1
                : 0);
    printKv("threshold.min_duration_ms", detectorReport.thresholds.minDurationMs);
    printKv("threshold.max_duration_ms", detectorReport.thresholds.maxDurationMs);
    printKv("aggregate.accepted_count", detectorReport.aggregates.acceptedCount);
    printKv("aggregate.rejected_count", detectorReport.aggregates.rejectedCount);
    serialOut().println();
}

void printDetectorDetailLine(const char* prefix, const DetectorReport* report) {
    if (report == nullptr) {
        return;
    }

    switch (report->detectorId) {
        case DetectorId::ScalarTransient:
            scalar::printScalarTransientDetailLine(prefix, report);
            break;
        case DetectorId::Unknown:
        default:
            break;
    }
}

} // namespace detection
