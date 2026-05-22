#pragma once

#include "AnalyzerReporting.h"
#include "../../detection/detectors/AmpTransientDetector.h"

struct AnalyzerSequenceClassificationInput {
    AnalyzerResult result = AnalyzerResult::Unknown;
    long dtMs = -1;
    float confidence = 0.0f;
    unsigned long rawCandidateCount = 0;
    AmpTransientDetector::TransientRejectReason strongestRejectReason = AmpTransientDetector::TransientRejectReason::None;
    bool audioOverflow = false;
    bool patternAvailable = false;
};

AnalyzerReason analyzerReasonFromSequenceOutcome(const AnalyzerSequenceClassificationInput& input);
AnalyzerClassification classifySequenceTrial(const AnalyzerSequenceClassificationInput& input);
