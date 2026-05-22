#include "AnalyzerClassifier.h"

#include "../../detection/detectors/AmpTransientDetector.h"

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
            switch (input.strongestRejectReason) {
                case AmpTransientDetector::TransientRejectReason::DurationTooShort:
                case AmpTransientDetector::TransientRejectReason::DurationTooLong:
                case AmpTransientDetector::TransientRejectReason::StrengthTooLow:
                    return AnalyzerReason::OccurrenceSeenButRejected;
                case AmpTransientDetector::TransientRejectReason::PeakStillActive:
                    return AnalyzerReason::InspectionFailed;
                case AmpTransientDetector::TransientRejectReason::None:
                default:
                    break;
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

AnalyzerClassification classifySequenceTrial(const AnalyzerSequenceClassificationInput& input) {
    AnalyzerClassification classification = {};
    classification.result = input.result;
    classification.reason = input.patternAvailable
        ? analyzerReasonFromSequenceOutcome(input)
        : AnalyzerReason::MissingPipelineResult;
    classification.dtMs = input.dtMs;
    classification.confidence = input.patternAvailable ? input.confidence : 0.0f;
    return classification;
}

