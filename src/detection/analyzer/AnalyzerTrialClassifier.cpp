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
                return AnalyzerReason::MissingEvaluatorReport;
            }
            if (input.sourceCandidateCount == 0) {
                return AnalyzerReason::NoOccurrence;
            }
            return AnalyzerReason::PipelineIncomplete;
        case AnalyzerResult::Rejected:
            if (input.sourceRejectedCount > 0 || input.detectorSelectedRejectPresent) {
                return AnalyzerReason::OccurrenceSeenButRejected;
            }
            if (input.patternAvailable) {
                return AnalyzerReason::PatternRejected;
            }
            if (input.sourceAcceptedCount > 0 || input.detectorAcceptedPresent) {
                return AnalyzerReason::MissingEvaluatorReport;
            }
            return input.sourceCandidateCount == 0 ? AnalyzerReason::NoOccurrence : AnalyzerReason::PipelineIncomplete;
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
        case AnalyzerReason::PipelineIncomplete:
        case AnalyzerReason::MissingInspectionReport:
        case AnalyzerReason::MissingEvaluatorReport:
        case AnalyzerReason::UncorrelatedPipelineEvent:
        case AnalyzerReason::UnknownStageFailure:
        case AnalyzerReason::NoOccurrence:
        case AnalyzerReason::OccurrenceSeenButRejected:
            return AnalyzerStage::Source;
        case AnalyzerReason::InspectionFailed:
            return AnalyzerStage::Inspect;
        case AnalyzerReason::PatternRejected:
        case AnalyzerReason::MultipleValidPatterns:
        case AnalyzerReason::MultipleCompetingPatterns:
            return AnalyzerStage::Evaluator;
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

AnalyzerResult sequenceTrialResult(const AnalyzerSequenceClassificationInput& input) {
    AnalyzerResult result = input.selectedResult;
    if (!input.patternAvailable && input.selectionNeedsVerdict) {
        result = AnalyzerResult::Miss;
    } else if (input.patternAvailable && !input.verdictValid) {
        result = AnalyzerResult::Rejected;
    }
    if (result == AnalyzerResult::Expected || result == AnalyzerResult::Late) {
        return result;  // a valid verdict in (or after) the window stays
    }
    if (input.rejectedInWindow) {
        return AnalyzerResult::Rejected;
    }
    if (input.selectedResult == AnalyzerResult::Unexpected) {
        return AnalyzerResult::Unexpected;
    }
    return AnalyzerResult::Miss;
}

AnalyzerClassification classifySequenceTrial(const AnalyzerSequenceClassificationInput& input) {
    AnalyzerSequenceClassificationInput resolved = input;
    resolved.result = sequenceTrialResult(input);
    AnalyzerClassification classification = {};
    classification.result = resolved.result;
    classification.reason = (resolved.patternAvailable || resolved.detectorReportAvailable || resolved.pipelineQueueOverflowCount > 0)
        ? analyzerReasonFromSequenceOutcome(resolved)
        : AnalyzerReason::MissingPipelineResult;
    classification.primaryStage = analyzerPrimaryStageFromReason(classification.reason);
    classification.dtMs = resolved.dtMs;
    if (resolved.patternAvailable && !resolved.verdictValid) {
        // The verdict is the pattern's own word: an invalid one makes the trial Rejected at the evaluator, whatever
        // the window held.
        classification.result = AnalyzerResult::Rejected;
        classification.reason = AnalyzerReason::PatternRejected;
        classification.primaryStage = AnalyzerStage::Evaluator;
    }
    return classification;
}
