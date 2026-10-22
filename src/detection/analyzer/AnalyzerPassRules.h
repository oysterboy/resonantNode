#pragma once

#include "AnalyzerReportTypes.h"
#include "../occurrences/OccurrenceIds.h"

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

// Moved to occurrences/OccurrenceIds.h (step 10, ANA-003); kept visible here
// for the Analyzer code and tests that name it detection::analyzer::.
using ::detection::nextOccurrenceId;

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
