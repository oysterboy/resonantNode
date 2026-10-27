#include "DetectorReportPrinter.h"

#include <Arduino.h>

#include "../../../detection/detectors/DetectorNames.h"
#include "ScalarTransientPrinter.h"
#include "../../../app/SerialOut.h"

// Shared detector report rendering. Dispatches to detector-owned detail printers.
namespace detection {

void printDetectorReportGenericLine(const char* prefix, unsigned long trial, const DetectorReport* report) {
    static const DetectorReport kEmptyDetectorReport = {};
    const auto& detectorReport = report != nullptr ? *report : kEmptyDetectorReport;
    const auto& accepted = detectorReport.accepted;
    const auto& selectedReject = detectorReport.selectedReject;

    serialOut().print(prefix);
    serialOut().print(" trial=");
    serialOut().print(trial);
    serialOut().print(" detector=");
    serialOut().print(detectorIdName(detectorReport.detectorId));
    serialOut().print(" source.selection=");
    serialOut().print(detectorReport.sourceSelection != nullptr ? detectorReport.sourceSelection : "none");
    serialOut().print(" source.occurrence_id=");
    serialOut().print(detectorReport.sourceOccurrenceId);
    serialOut().print(" source.candidate_id=");
    serialOut().print(detectorReport.sourceCandidateId);
    serialOut().print(" source.report_matched=");
    serialOut().print(detectorReport.sourceReportMatched ? 1 : 0);
    serialOut().print(" window.start_ms=");
    serialOut().print(detectorReport.reportStartMs);
    serialOut().print(" window.end_ms=");
    serialOut().print(detectorReport.reportEndMs);
    serialOut().print(" accepted.present=");
    serialOut().print(accepted.present ? 1 : 0);
    serialOut().print(" accepted.start_ms=");
    serialOut().print(accepted.startMs);
    serialOut().print(" accepted.peak_ms=");
    serialOut().print(accepted.peakMs);
    serialOut().print(" accepted.end_ms=");
    serialOut().print(accepted.endMs);
    serialOut().print(" accepted.duration_ms=");
    serialOut().print(accepted.durationMs);
    serialOut().print(" accepted.strength=");
    serialOut().print(accepted.strength, 1);
    serialOut().print(" accepted.peak=");
    serialOut().print(accepted.peak, 1);
    serialOut().print(" accepted.mean=");
    serialOut().print(accepted.mean, 1);
    serialOut().print(" accepted.rms=");
    serialOut().print(accepted.rms, 1);
    serialOut().print(" accepted.coverage_attack_ms=");
    serialOut().print(accepted.coverageAboveAttackMs);
    serialOut().print(" accepted.coverage_release_ms=");
    serialOut().print(accepted.coverageAboveReleaseMs);
    serialOut().print(" accepted.sustained_ms=");
    serialOut().print(accepted.sustainedMs);
    serialOut().print(" accepted.island_count=");
    serialOut().print(accepted.islandCount);
    serialOut().print(" accepted.gap_count=");
    serialOut().print(accepted.gapCount);
    serialOut().print(" accepted.island_max_ms=");
    serialOut().print(accepted.islandMaxMs);
    serialOut().print(" accepted.gap_max_ms=");
    serialOut().print(accepted.gapMaxMs);
    serialOut().print(" reject.present=");
    serialOut().print(selectedReject.present ? 1 : 0);
    serialOut().print(" reject.class=");
    serialOut().print(detectorRejectClassName(selectedReject.rejectClass));
    serialOut().print(" reject.detector_reason=");
    serialOut().print(selectedReject.detectorReason != nullptr ? selectedReject.detectorReason : "none");
    serialOut().print(" reject.start_ms=");
    serialOut().print(selectedReject.startMs);
    serialOut().print(" reject.peak_ms=");
    serialOut().print(selectedReject.peakMs);
    serialOut().print(" reject.end_ms=");
    serialOut().print(selectedReject.endMs);
    serialOut().print(" reject.duration_ms=");
    serialOut().print(selectedReject.durationMs);
    serialOut().print(" reject.strength=");
    serialOut().print(selectedReject.strength, 1);
    serialOut().print(" reject.peak=");
    serialOut().print(selectedReject.peak, 1);
    serialOut().print(" reject.mean=");
    serialOut().print(selectedReject.mean, 1);
    serialOut().print(" reject.rms=");
    serialOut().print(selectedReject.rms, 1);
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
    serialOut().print(" candidate.mean_strength=");
    serialOut().print(candidateMeanStrength, 1);
    serialOut().print(" candidate.min_strength=");
    serialOut().print(candidateMatchedMeanStrength, 1);
    serialOut().print(" candidate.strength_count=");
    serialOut().print(candidateStrengthCount);
    serialOut().print(" candidate.matched_strength_count=");
    serialOut().print(candidateMatchedStrengthCount);
    serialOut().print(" reject.coverage_attack_ms=");
    serialOut().print(selectedReject.coverageAboveAttackMs);
    serialOut().print(" reject.coverage_release_ms=");
    serialOut().print(selectedReject.coverageAboveReleaseMs);
    serialOut().print(" reject.sustained_ms=");
    serialOut().print(selectedReject.sustainedMs);
    serialOut().print(" reject.island_count=");
    serialOut().print(selectedReject.islandCount);
    serialOut().print(" reject.gap_count=");
    serialOut().print(selectedReject.gapCount);
    serialOut().print(" reject.island_max_ms=");
    serialOut().print(selectedReject.islandMaxMs);
    serialOut().print(" reject.gap_max_ms=");
    serialOut().print(selectedReject.gapMaxMs);
    serialOut().print(" carrier.quality_pass=");
    if (detectorReport.detectorId == DetectorId::ScalarTransient) {
        const bool carrierQualityPass = detectorReport.scalar.inspect.carrierQualityRequired
            ? (detectorReport.scalar.inspect.carrierCoveragePassed &&
               detectorReport.scalar.inspect.carrierIslandPassed &&
               detectorReport.scalar.inspect.carrierGapPassed)
            : true;
        serialOut().print(carrierQualityPass ? 1 : 0);
    } else if (accepted.present) {
        serialOut().print(1);
    } else {
        serialOut().print(0);
    }
    serialOut().print(" candidate.min_strength_pass=");
    serialOut().print(detectorReport.detectorId == DetectorId::ScalarTransient &&
        detectorReport.scalar.inspect.matchedMeanPassed ? 1 : 0);
    serialOut().print(" threshold.min_duration_ms=");
    serialOut().print(detectorReport.thresholds.minDurationMs);
    serialOut().print(" threshold.max_duration_ms=");
    serialOut().print(detectorReport.thresholds.maxDurationMs);
    serialOut().print(" aggregate.accepted_count=");
    serialOut().print(detectorReport.aggregates.acceptedCount);
    serialOut().print(" aggregate.rejected_count=");
    serialOut().println(detectorReport.aggregates.rejectedCount);
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
