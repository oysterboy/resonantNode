#pragma once

namespace detection {

// The next occurrence id after `currentOccurrenceId`; ids start at 1, 0 means
// "none". A detector identity rule, used by the scalar detector in every build.
// It lived in analyzer/AnalyzerPassRules.h until step 10 (ANA-003), which made
// every build's detector compile the Analyzer report types.
inline unsigned long nextOccurrenceId(unsigned long currentOccurrenceId) {
    return currentOccurrenceId == 0 ? 1UL : currentOccurrenceId + 1UL;
}

}  // namespace detection
