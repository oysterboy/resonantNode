#pragma once

#include "AnalyzerReportingTypes.h"

/*
AnalyzerClassifier

Generic Analyzer trial classification bridge.
Consumes the small Analyzer-owned outcome summary and maps it into generic
AnalyzerReason / AnalyzerStage values only.
Detector-specific, occurrence-specific, and pattern-specific reject detail
must stay on DetectorReport, Occurrence/InspectedOccurrence, and PatternResult.
*/
struct AnalyzerSequenceClassificationInput {
    // Generic trial result from Analyzer trial control flow.
    AnalyzerResult result = AnalyzerResult::Unknown;
    // Generic timing delta for the selected trial PatternResult when present.
    long dtMs = -1;
    // Runtime-private candidate count used by trial classification.
    unsigned long rawCandidateCount = 0;
    // Runtime-private overflow flag.
    bool audioOverflow = false;
    // Canonical PatternResult availability for the finalized trial snapshot.
    // This remains generic; detector/pattern-specific reasons live elsewhere.
    bool patternAvailable = false;
    // Canonical DetectorReport availability for the active detector path.
    bool detectorReportAvailable = false;
    bool detectorAcceptedPresent = false;
    bool detectorSelectedRejectPresent = false;
};

AnalyzerReason analyzerReasonFromSequenceOutcome(const AnalyzerSequenceClassificationInput& input);
AnalyzerClassification classifySequenceTrial(const AnalyzerSequenceClassificationInput& input);
