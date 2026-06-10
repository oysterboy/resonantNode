#pragma once

#include "AnalyzerLegacyReporting.h"

/*
AnalyzerClassifier

Legacy Analyzer classification bridge.
Consumes AnalyzerResult plus trial metadata and maps it into AnalyzerReason
without becoming the canonical detector contract.
Keep this small and internal until the output rebuild replaces it.
*/
struct AnalyzerSequenceClassificationInput {
    // Legacy trial result from the current Analyzer pipeline.
    AnalyzerResult result = AnalyzerResult::Unknown;
    // Legacy time delta used by the old reason mapping.
    long dtMs = -1;
    // Legacy candidate count from sequence bookkeeping.
    unsigned long rawCandidateCount = 0;
    // Legacy overflow flag from the current Analyzer path.
    bool audioOverflow = false;
    // Legacy gate: false keeps MissingPipelineResult semantics.
    bool patternAvailable = false;
};

AnalyzerReason analyzerReasonFromSequenceOutcome(const AnalyzerSequenceClassificationInput& input);
AnalyzerClassification classifySequenceTrial(const AnalyzerSequenceClassificationInput& input);
