#pragma once

#include "AnalyzerReporting.h"

/*
AnalyzerClassifier

Small helper for Analyzer trial classification reasons.
Consumes AnalyzerResult plus trial metadata.
Does not re-evaluate DetectionRuntime pattern validity.
The AMP transient reject reason is used only as a diagnostic reason input.
*/
struct AnalyzerSequenceClassificationInput {
    AnalyzerResult result = AnalyzerResult::Unknown;
    long dtMs = -1;
    float confidence = 0.0f;
    unsigned long rawCandidateCount = 0;
    bool audioOverflow = false;
    bool patternAvailable = false;
};

AnalyzerReason analyzerReasonFromSequenceOutcome(const AnalyzerSequenceClassificationInput& input);
AnalyzerClassification classifySequenceTrial(const AnalyzerSequenceClassificationInput& input);
