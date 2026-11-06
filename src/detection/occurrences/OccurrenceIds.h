#pragma once

namespace detection {

// The next occurrence id after `currentOccurrenceId`; ids start at 1, 0 means
// "none". A detector identity rule, used by the scalar detector in every build.
// It lives here, not in analyzer/AnalyzerPassRules.h: every build's detector would
// otherwise compile the Analyzer report types (ANA-003).
inline unsigned long nextOccurrenceId(unsigned long currentOccurrenceId) {
    return currentOccurrenceId == 0 ? 1UL : currentOccurrenceId + 1UL;
}

}  // namespace detection
