#pragma once

#include "AnalyzerReportTypes.h"

namespace detection::analyzer {

inline bool sourceReportMatchesIdentity(unsigned long recordOccurrenceId,
                                        unsigned long selectedOccurrenceId,
                                        unsigned long recordStartMs,
                                        unsigned long selectedStartMs,
                                        unsigned long recordEndMs,
                                        unsigned long selectedEndMs) {
    return recordOccurrenceId == selectedOccurrenceId &&
           recordStartMs == selectedStartMs &&
           recordEndMs == selectedEndMs;
}

inline const char* sourceReportReason(bool sourceReportMatched, bool hasDetectorReport) {
    return sourceReportMatched ? "none" : (hasDetectorReport ? "id_mismatch" : "none");
}

inline unsigned long nextOccurrenceId(unsigned long currentOccurrenceId) {
    return currentOccurrenceId == 0 ? 1UL : currentOccurrenceId + 1UL;
}

inline unsigned int completedPrimaryTrialCount(const AnalyzerCleanSummary& summary) {
    return summary.expectedTrials +
           summary.earlyTrials +
           summary.lateTrials +
           summary.missTrials +
           summary.duplicateTrials +
           summary.unexpectedTrials +
           summary.rejectedTrials +
           summary.ambiguousTrials +
           summary.tooDenseTrials;
}

inline bool patternRejectedTrial(const AnalyzerReport& report) {
    return report.classification.result == AnalyzerResult::Rejected &&
           report.classification.reason == AnalyzerReason::PatternRejected;
}

} // namespace detection::analyzer
