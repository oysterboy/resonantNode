#include "AnalyzerTrialClassifier.h"

AnalyzerReason analyzerReasonFromSequenceOutcome(const AnalyzerSequenceClassificationInput& input) {
    if (input.pipelineQueueOverflowCount > 0) {
        return AnalyzerReason::PipelineQueueOverflow;
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
        case AnalyzerResult::Miss:
            if (input.sourceRejectedCount > 0 || input.detectorSelectedRejectPresent) {
                return AnalyzerReason::OccurrenceSeenButRejected;
            }
            if (input.sourceAcceptedCount > 0 || input.detectorAcceptedPresent) {
                return AnalyzerReason::InspectionFailed;
            }
            if (input.sourceCandidateCount == 0) {
                return AnalyzerReason::NoOccurrence;
            }
            return AnalyzerReason::MissingPipelineResult;
        case AnalyzerResult::Rejected:
            if (input.patternInspectionFailed) {
                return AnalyzerReason::InspectionFailed;
            }
            if (input.sourceRejectedCount > 0 || input.detectorSelectedRejectPresent) {
                return AnalyzerReason::OccurrenceSeenButRejected;
            }
            if (input.patternAvailable) {
                return AnalyzerReason::PatternRejected;
            }
            if (input.sourceAcceptedCount > 0 || input.detectorAcceptedPresent) {
                return AnalyzerReason::InspectionFailed;
            }
            return input.sourceCandidateCount == 0 ? AnalyzerReason::NoOccurrence : AnalyzerReason::MissingPipelineResult;
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
        case AnalyzerReason::PipelineQueueOverflow:
        case AnalyzerReason::NoOccurrence:
        case AnalyzerReason::OccurrenceSeenButRejected:
            return AnalyzerStage::Source;
        case AnalyzerReason::InspectionFailed:
            return AnalyzerStage::Inspect;
        case AnalyzerReason::PatternRejected:
        case AnalyzerReason::MultipleValidPatterns:
        case AnalyzerReason::MultipleCompetingPatterns:
            return AnalyzerStage::Pattern;
        case AnalyzerReason::FieldTooDense:
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
    classification.reason = (input.patternAvailable || input.detectorReportAvailable || input.pipelineQueueOverflowCount > 0)
        ? analyzerReasonFromSequenceOutcome(input)
        : AnalyzerReason::MissingPipelineResult;
    classification.primaryStage = analyzerPrimaryStageFromReason(classification.reason);
    classification.dtMs = input.dtMs;
    return classification;
}
