#include "DetectorReportPrinter.h"

#include <Arduino.h>

#include "../../../detection/detectors/DetectorNames.h"
#include "ScalarTransientPrinter.h"
#include "../../../app/SerialOut.h"

#include "../../../app/PrintKeyValue.h"
// Shared detector report rendering. Dispatches to detector-owned detail printers.
namespace detection {

void printDetectorReportGenericLine(const char* prefix, unsigned long trial, const DetectorReport* report) {
    static const DetectorReport kEmptyDetectorReport = {};
    const auto& detectorReport = report != nullptr ? *report : kEmptyDetectorReport;
    const auto& accepted = detectorReport.accepted;
    const auto& selectedReject = detectorReport.selectedReject;

    serialOut().print(prefix);
    printKeyValue("trial", trial);
    printKeyValue("detector", detectorIdName(detectorReport.detectorId));
    printKeyValue("source.selection",
                  detectorReport.sourceSelection != nullptr ? detectorReport.sourceSelection : "none");
    printKeyValue("source.occurrence_id", detectorReport.sourceOccurrenceId);
    printKeyValue("source.candidate_id", detectorReport.sourceCandidateId);
    printKeyValue("source.report_matched", detectorReport.sourceReportMatched ? 1 : 0);
    printKeyValue("window.start_ms", detectorReport.reportStartMs);
    printKeyValue("window.end_ms", detectorReport.reportEndMs);
    printKeyValue("accepted.present", accepted.present ? 1 : 0);
    printKeyValue("accepted.start_ms", accepted.startMs);
    printKeyValue("accepted.peak_ms", accepted.peakMs);
    printKeyValue("accepted.end_ms", accepted.endMs);
    printKeyValue("accepted.duration_ms", accepted.durationMs);
    printKeyValue("accepted.strength", accepted.strength, 1);
    printKeyValue("accepted.peak", accepted.peak, 1);
    printKeyValue("accepted.mean", accepted.mean, 1);
    printKeyValue("accepted.rms", accepted.rms, 1);
    printKeyValue("accepted.coverage_attack_ms", accepted.coverageAboveAttackMs);
    printKeyValue("accepted.coverage_release_ms", accepted.coverageAboveReleaseMs);
    printKeyValue("accepted.sustained_ms", accepted.sustainedMs);
    printKeyValue("accepted.island_count", accepted.islandCount);
    printKeyValue("accepted.gap_count", accepted.gapCount);
    printKeyValue("accepted.island_max_ms", accepted.islandMaxMs);
    printKeyValue("accepted.gap_max_ms", accepted.gapMaxMs);
    printKeyValue("reject.present", selectedReject.present ? 1 : 0);
    printKeyValue("reject.class", detectorRejectClassName(selectedReject.rejectClass));
    printKeyValue("reject.detector_reason",
                  selectedReject.detectorReason != nullptr ? selectedReject.detectorReason : "none");
    printKeyValue("reject.start_ms", selectedReject.startMs);
    printKeyValue("reject.peak_ms", selectedReject.peakMs);
    printKeyValue("reject.end_ms", selectedReject.endMs);
    printKeyValue("reject.duration_ms", selectedReject.durationMs);
    printKeyValue("reject.strength", selectedReject.strength, 1);
    printKeyValue("reject.peak", selectedReject.peak, 1);
    printKeyValue("reject.mean", selectedReject.mean, 1);
    printKeyValue("reject.rms", selectedReject.rms, 1);
    const bool useAcceptedCandidate = accepted.present || !selectedReject.present;
    const float candidateMeanStrength = useAcceptedCandidate ? accepted.meanStrength : selectedReject.meanStrength;
    const float candidateMatchedMeanStrength =
        useAcceptedCandidate ? accepted.matchedMeanStrength : selectedReject.matchedMeanStrength;
    const unsigned int candidateStrengthCount =
        useAcceptedCandidate ? accepted.strengthCount : selectedReject.strengthCount;
    const unsigned int candidateMatchedStrengthCount =
        useAcceptedCandidate ? accepted.matchedStrengthCount : selectedReject.matchedStrengthCount;
    printKeyValue("candidate.mean_strength", candidateMeanStrength, 1);
    printKeyValue("candidate.min_strength", candidateMatchedMeanStrength, 1);
    printKeyValue("candidate.strength_count", candidateStrengthCount);
    printKeyValue("candidate.matched_strength_count", candidateMatchedStrengthCount);
    printKeyValue("reject.coverage_attack_ms", selectedReject.coverageAboveAttackMs);
    printKeyValue("reject.coverage_release_ms", selectedReject.coverageAboveReleaseMs);
    printKeyValue("reject.sustained_ms", selectedReject.sustainedMs);
    printKeyValue("reject.island_count", selectedReject.islandCount);
    printKeyValue("reject.gap_count", selectedReject.gapCount);
    printKeyValue("reject.island_max_ms", selectedReject.islandMaxMs);
    printKeyValue("reject.gap_max_ms", selectedReject.gapMaxMs);
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
    printKeyValue("candidate.min_strength_pass",
                  detectorReport.detectorId == DetectorId::ScalarTransient &&
                          detectorReport.scalar.inspect.matchedMeanPassed
                      ? 1
                      : 0);
    printKeyValue("threshold.min_duration_ms", detectorReport.thresholds.minDurationMs);
    printKeyValue("threshold.max_duration_ms", detectorReport.thresholds.maxDurationMs);
    printKeyValue("aggregate.accepted_count", detectorReport.aggregates.acceptedCount);
    printKeyValue("aggregate.rejected_count", detectorReport.aggregates.rejectedCount);
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
