#pragma once

#include "AnalyzerReportTypes.h"

/*
AnalyzerClassifier

Generic Analyzer trial classification bridge.
Consumes the small Analyzer-owned outcome summary and maps it into generic
AnalyzerReason / AnalyzerStage values only.
Detector-specific, occurrence-specific, and pattern-specific reject detail
must stay on DetectorReport, Occurrence/InspectedOccurrence, and OccurrenceVerdict.
*/
// A valid verdict this long after the trigger is Late, not Expected. One
// definition since step 10 (it was a constant in three Analyzer files).
constexpr long kAnalyzerLateOnsetMinMs = 200L;

struct AnalyzerSequenceClassificationInput {
    // The trial's selection (what the Analyzer's trial selection found in the window; 11g C5 moved the result ladder
    // that turns it into the trial result from finalizeSequenceTrial into sequenceTrialResult() below).
    AnalyzerResult selectedResult = AnalyzerResult::Unknown;
    // The selection is of a kind that needs a verdict (a valid pattern or an accepted occurrence).
    bool selectionNeedsVerdict = false;
    // The selected verdict's validity (meaningful when patternAvailable).
    bool verdictValid = false;
    // A candidate was rejected inside the trial's window.
    bool rejectedInWindow = false;
    // The trial result; classifySequenceTrial sets it from the fields above (callers leave it).
    AnalyzerResult result = AnalyzerResult::Unknown;
    // Generic timing delta for the selected trial OccurrenceVerdict when present.
    long dtMs = -1;
    // Trial-local pipeline evidence counts.
    unsigned long sourceCandidateCount = 0;
    unsigned long sourceAcceptedCount = 0;
    unsigned long sourceRejectedCount = 0;
    unsigned long inspectedOccurrenceCount = 0;
    unsigned long verdictCount = 0;
    unsigned long pipelineQueueOverflowCount = 0;
    // Runtime-private buffer overrun flag.
    bool bufferOverrun = false;
    // Canonical OccurrenceVerdict availability for the finalized trial snapshot.
    // This remains generic; detector/pattern-specific reasons live elsewhere.
    bool patternAvailable = false;
    // Canonical DetectorReport availability for the active detector path.
    bool detectorReportAvailable = false;
    bool detectorAcceptedPresent = false;
    bool detectorSelectedRejectPresent = false;
};

AnalyzerReason analyzerReasonFromSequenceOutcome(const AnalyzerSequenceClassificationInput& input);
// The trial result from the selection (review A8 / 11g C5: the one ladder; it was in finalizeSequenceTrial).
AnalyzerResult sequenceTrialResult(const AnalyzerSequenceClassificationInput& input);
// The whole classification, once: the result ladder, the reason and stage, and an invalid verdict as
// Rejected / PatternRejected / Evaluator (it was a second override in buildSequenceAnalyzerReport).
AnalyzerClassification classifySequenceTrial(const AnalyzerSequenceClassificationInput& input);
