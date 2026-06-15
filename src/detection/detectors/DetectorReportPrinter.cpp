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
