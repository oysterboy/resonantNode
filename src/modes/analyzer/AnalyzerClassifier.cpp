#include "AnalyzerClassifier.h"

AnalyzerReason analyzerReasonFromSequenceOutcome(const AnalyzerSequenceClassificationInput& input) {
    if (input.audioOverflow) {
        return AnalyzerReason::InvalidAudio;
    }

    switch (input.result) {
        case AnalyzerResult::Expected:
            return AnalyzerReason::ValidPatternInExpectedWindow;
        case AnalyzerResult::Late:
            return AnalyzerReason::ValidPatternAfterWindow;
        case AnalyzerResult::Unexpected:
            return AnalyzerReason::UnexpectedValidPatternWithoutTrigger;
        case AnalyzerResult::Duplicate:
            return AnalyzerReason::DuplicatePatternAfterPrimary;
        case AnalyzerResult::InvalidAudio:
            return AnalyzerReason::InvalidAudio;
        case AnalyzerResult::Miss:
            if (input.rawCandidateCount == 0) {
                return AnalyzerReason::NoOccurrence;
            }
            return input.dtMs >= 0 ? AnalyzerReason::PatternCandidateRejected : AnalyzerReason::NoOccurrence;
        case AnalyzerResult::Rejected:
            return AnalyzerReason::PatternCandidateRejected;
        case AnalyzerResult::Ambiguous:
            return AnalyzerReason::MultipleCompetingPatterns;
        case AnalyzerResult::TooDense:
            return AnalyzerReason::FieldTooDense;
        case AnalyzerResult::Unknown:
        default:
            return AnalyzerReason::Unknown;
    }
}

AnalyzerStage analyzerPrimaryStageFromReason(AnalyzerReason reason) {
    switch (reason) {
        case AnalyzerReason::MissingPipelineResult:
        case AnalyzerReason::NoOccurrence:
        case AnalyzerReason::OccurrenceSeenButRejected:
            return AnalyzerStage::Source;
        case AnalyzerReason::InspectionFailed:
            return AnalyzerStage::Inspect;
        case AnalyzerReason::PatternCandidateRejected:
        case AnalyzerReason::MultipleValidPatterns:
        case AnalyzerReason::MultipleCompetingPatterns:
            return AnalyzerStage::Pattern;
        case AnalyzerReason::FieldTooDense:
        case AnalyzerReason::InvalidAudio:
            return AnalyzerStage::Field;
        case AnalyzerReason::ValidPatternInExpectedWindow:
        case AnalyzerReason::ValidPatternBeforeWindow:
        case AnalyzerReason::ValidPatternAfterWindow:
        case AnalyzerReason::UnexpectedValidPatternWithoutTrigger:
        case AnalyzerReason::DuplicatePatternAfterPrimary:
            return AnalyzerStage::Analyzer;
        case AnalyzerReason::None:
        case AnalyzerReason::Unknown:
        default:
            return AnalyzerStage::None;
    }
}

AnalyzerClassification classifySequenceTrial(const AnalyzerSequenceClassificationInput& input) {
    AnalyzerClassification classification = {};
    classification.result = input.result;
    classification.reason = input.patternAvailable
        ? analyzerReasonFromSequenceOutcome(input)
        : AnalyzerReason::MissingPipelineResult;
    classification.primaryStage = analyzerPrimaryStageFromReason(classification.reason);
    classification.dtMs = input.dtMs;
    return classification;
}

