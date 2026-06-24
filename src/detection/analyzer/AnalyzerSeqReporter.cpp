#include "../../modes/analyzer/AnalyzerModeApp.h"

#include <Arduino.h>
#include <string.h>

#include "../../detection/detectors/DetectorNames.h"
#include "../../detection/detectors/DetectorReportPrinter.h"
#include "../../detection/inspection/InspectionNames.h"

namespace {

const char* strengthClassForObservationTarget(const AnalyzerReport& report, detection::InspectionTarget target) {
    for (size_t i = 0; i < report.profileDetail.inspectionObservationCount; ++i) {
        if (report.profileDetail.inspectionObservationTargets[i] == target) {
            return detection::strengthClassName(report.profileDetail.inspectionObservations[i].strength);
        }
    }
    return "unknown";
}

const char* pipelineIntegrityReasonName(detection::PipelineIntegrityReason reason) {
    switch (reason) {
        case detection::PipelineIntegrityReason::None:
            return "none";
        case detection::PipelineIntegrityReason::MissingDetectorReport:
            return "missing_detector_report";
        case detection::PipelineIntegrityReason::MissingInspectedOccurrence:
            return "missing_inspected_occurrence";
        case detection::PipelineIntegrityReason::MissingPatternResult:
            return "missing_pattern_result";
        case detection::PipelineIntegrityReason::OccurrenceIdMismatch:
            return "occurrence_id_mismatch";
        case detection::PipelineIntegrityReason::InspectionQueueOverflow:
            return "inspection_queue_overflow";
        case detection::PipelineIntegrityReason::PatternResultQueueOverflow:
            return "pattern_result_queue_overflow";
        case detection::PipelineIntegrityReason::PipelineEventQueueOverflow:
            return "pipeline_event_queue_overflow";
        default:
            return "unknown";
    }
}

void printCanonicalStageLine(const char* prefix, const AnalyzerReport& report, bool extended) {
    Serial.print(prefix);
    Serial.print(" expected.start_ms=");
    Serial.print(report.expected.windowStartMs);
    Serial.print(" expected.end_ms=");
    Serial.print(report.expected.windowEndMs);
    Serial.print(" pattern.type=");
    Serial.print(report.primaryPattern.type != nullptr ? report.primaryPattern.type : "unknown");
    Serial.print(" pattern.valid=");
    Serial.print(report.primaryPattern.accepted ? 1 : 0);
    Serial.print(" pattern.reason=");
    Serial.print(report.primaryPattern.reason != nullptr ? report.primaryPattern.reason : "none");
    Serial.print(" analyzer.result=");
    Serial.print(analyzerResultName(report.classification.result));
    Serial.print(" analyzer.stage=");
    Serial.print(analyzerStageName(report.classification.primaryStage));
    Serial.print(" analyzer.reason=");
    Serial.print(analyzerReasonName(report.classification.reason));
    Serial.print(" analyzer.dt_ms=");
    Serial.print(report.classification.dtMs);
    if (extended) {
        Serial.print(" occurrence.present=");
        Serial.print(report.occurrences.present ? 1 : 0);
        Serial.print(" occurrence.source=");
        Serial.print(report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "none");
        Serial.print(" inspection.label=");
        Serial.print(detection::inspectionTargetName(report.inspection.moduleTarget));
        Serial.print(" inspection.strength=");
        Serial.print(report.inspection.moduleStrengthClass != nullptr ? report.inspection.moduleStrengthClass : "unknown");
        Serial.print(" integrity.complete=");
        Serial.print(report.integrity.correlationComplete ? 1 : 0);
        Serial.print(" integrity.reason=");
        Serial.print(report.integrity.reason != nullptr ? report.integrity.reason : "none");
    }
    Serial.println();
}

}

/*
Canonical analyzer reporting surface.
*/

void AnalyzerApp::printSequenceTrialHeader(unsigned long trialNumber) const {
    Serial.println();
    Serial.print("#");
    Serial.print(trialNumber);
    Serial.print(" ----------------");
    Serial.println();
}

void AnalyzerApp::printSequenceTrial(const AnalyzerReport& report) const {
    const char* trialRejectReason = analyzerReasonName(report.classification.reason);
    if (report.classification.result == AnalyzerResult::Rejected &&
        report.occurrences.rejectReason != nullptr &&
        report.occurrences.rejectReason[0] != '\0' &&
        strcmp(report.occurrences.rejectReason, "none") != 0 &&
        strcmp(report.occurrences.rejectReason, trialRejectReason) != 0) {
        trialRejectReason = report.occurrences.rejectReason;
    }

    Serial.print("SEQ_TRIAL trial=");
    Serial.print(report.context.trial);
    Serial.print(" result=");
    Serial.print(analyzerResultName(report.classification.result));
    Serial.print(" stage=");
    Serial.print(analyzerStageName(report.classification.primaryStage));
    Serial.print(" reject_reason=");
    Serial.print(trialRejectReason);
    Serial.print(" source_selection=");
    Serial.print(report.sourceSelection != nullptr ? report.sourceSelection : "none");
    Serial.print(" source_reason=");
    Serial.print(report.sourceReportReason != nullptr ? report.sourceReportReason : "none");
    Serial.print(" dt=");
    if (report.classification.result == AnalyzerResult::Rejected) {
        Serial.print("na");
    } else if (report.classification.dtMs >= 0) {
        Serial.print(report.classification.dtMs);
        Serial.print("ms");
    } else {
        Serial.print(report.classification.dtMs);
        Serial.print("ms");
    }
    if (report.occurrences.primaryDurationMs > 0) {
        Serial.print(" duration_ms=");
        Serial.print(report.occurrences.primaryDurationMs);
    } else if (report.detectorReport != nullptr && report.detectorReport->accepted.present) {
        Serial.print(" duration_ms=");
        Serial.print(report.detectorReport->accepted.durationMs);
    }
    Serial.print(" contrast_class=");
    Serial.print(strengthClassForObservationTarget(report, detection::InspectionTarget::Contrast));
    Serial.print(" amp_class=");
    Serial.print(strengthClassForObservationTarget(report, detection::InspectionTarget::Amp));
    if (report.detectorReport != nullptr) {
        const auto& accepted = report.detectorReport->accepted;
        const auto& selectedReject = report.detectorReport->selectedReject;
        const bool useAccepted = accepted.present;
        const auto coverageMs = useAccepted ? accepted.coverageAboveReleaseMs : selectedReject.coverageAboveReleaseMs;
        const auto islandMaxMs = useAccepted ? accepted.islandMaxMs : selectedReject.islandMaxMs;
        const auto gapMaxMs = useAccepted ? accepted.gapMaxMs : selectedReject.gapMaxMs;
        Serial.print(" coverage_ms=");
        Serial.print(coverageMs);
        Serial.print(" island_max_ms=");
        Serial.print(islandMaxMs);
        Serial.print(" gap_max_ms=");
        Serial.print(gapMaxMs);
    }
    Serial.print(" source_strength=");
    Serial.print(report.occurrences.primaryStrength, 1);
    Serial.print(" pattern_confidence=");
    Serial.print(report.primaryPattern.confidence, 2);
    Serial.print(" dup=");
    Serial.print(report.debug.duplicates);
    Serial.print(" src_total=");
    Serial.print(report.occurrences.total);
    Serial.print(" src_acc=");
    Serial.print(report.occurrences.accepted);
    Serial.print(" src_rej=");
    Serial.print(report.occurrences.rejected);
    Serial.print(" buffer_overrun=");
    Serial.print(report.debug.bufferOverrun ? 1 : 0);
    Serial.print(" profile=");
    Serial.print(report.context.profile != nullptr ? report.context.profile : "unknown");
    Serial.println();
}

void AnalyzerApp::printSequenceInspectCanonical(const AnalyzerReport& report) const {
    if (!shouldPrintSequenceInspect(report)) {
        return;
    }

    const size_t observationCount = report.profileDetail.inspectionObservationCount;

    auto printInspectCore = [&report](const detection::ScalarInspectionObservation* observation,
                                      detection::InspectionTarget target) {
        const bool available = observation != nullptr && observation->available;
        Serial.print("SEQ_INSPECT");
        Serial.print(" inspect.label=");
        Serial.print(detection::inspectionTargetName(target));
        Serial.print(" inspect.anchor_ms=");
        Serial.print(observation != nullptr ? observation->anchorMs : 0UL);
        Serial.print(" inspect.requested_start_ms=");
        Serial.print(observation != nullptr ? observation->requestedStartMs : 0UL);
        Serial.print(" inspect.requested_end_ms=");
        Serial.print(observation != nullptr ? observation->requestedEndMs : 0UL);
        Serial.print(" inspect.inspection_now_ms=");
        Serial.print(observation != nullptr ? observation->inspectionNowMs : 0UL);
        Serial.print(" inspect.available_start_ms=");
        Serial.print(observation != nullptr ? observation->availableStartMs : 0UL);
        Serial.print(" inspect.available_end_ms=");
        Serial.print(observation != nullptr ? observation->availableEndMs : 0UL);
        Serial.print(" inspect.value=");
        Serial.print(available ? observation->classificationValue : 0.0f, 3);
        Serial.print(" inspect.strength_class=");
        Serial.print(available ? detection::strengthClassName(observation->strength) : "unknown");
        Serial.print(" inspect.valid=");
        Serial.print(available ? 1 : 0);
        Serial.print(" inspect.status=");
        Serial.print(available ? "observed" : "missing");
        Serial.print(" inspect.reject_reason=");
        Serial.print(available ? "none" : detection::scalarInspectionNoteName(observation != nullptr ? observation->note : detection::ScalarInspectionNote::WindowInvalid));
        Serial.print(" inspect.stream=");
        Serial.print(observation != nullptr ? scalarObservedStreamDisplayName(observation->stream) : "unknown");
        Serial.print(" inspect.metric=");
        Serial.print(observation != nullptr ? detection::scalarInspectionModeName(observation->mode) : "unknown");
        Serial.print(" inspect.window_ms=");
        Serial.print(observation != nullptr ? observation->windowMs : 0UL);
        Serial.print(" inspect.expected_bin_count=");
        Serial.print(observation != nullptr ? observation->windowMs : 0UL);
        Serial.print(" inspect.sample_count=");
        Serial.print(observation != nullptr ? static_cast<unsigned long>(observation->sampleCount) : 0UL);
        Serial.print(" inspect.coverage=");
        Serial.print(observation != nullptr ? observation->coverageRatio : 0.0f, 3);
        Serial.print(" inspect.covered_ms=");
        Serial.print(observation != nullptr ? observation->coveredDurationMs : 0UL);
        Serial.print(" inspect.coverage_complete=");
        Serial.print(observation != nullptr && observation->coverageComplete ? 1 : 0);
        Serial.print(" inspect.future_unavailable=");
        Serial.print(observation != nullptr && observation->requestedFutureAtInspection ? 1 : 0);
        Serial.print(" inspect.peak=");
        Serial.print(observation != nullptr ? observation->peak : 0.0f, 3);
        Serial.print(" inspect.mean=");
        Serial.print(observation != nullptr ? observation->mean : 0.0f, 3);
        Serial.print(" inspect.rms=");
        Serial.print(observation != nullptr ? observation->rms : 0.0f, 3);
        Serial.print(" inspect.median=");
        Serial.print(observation != nullptr ? observation->median : 0.0f, 3);
        Serial.print(" inspect.p75=");
        Serial.print(observation != nullptr ? observation->p75 : 0.0f, 3);
        Serial.print(" inspect.p90=");
        Serial.print(observation != nullptr ? observation->p90 : 0.0f, 3);
        Serial.print(" inspect.trimmed_mean=");
        Serial.print(observation != nullptr ? observation->trimmedMean : 0.0f, 3);
        Serial.print(" inspect.input_value_count=");
        Serial.print(observation != nullptr ? static_cast<unsigned long>(observation->freshValueCount) : 0UL);
        Serial.print(" inspect.occurrence_id=");
        Serial.print(report.sourceOccurrenceId);
        Serial.print(" inspect.integrity_complete=");
        Serial.print(report.integrity.correlationComplete ? 1 : 0);
        Serial.print(" inspect.integrity_reason=");
        Serial.print(report.integrity.reason != nullptr ? report.integrity.reason : "none");
        Serial.print(" inspect.integrity_queue_overflow=");
        Serial.print(report.integrity.queueOverflowAffected ? 1 : 0);
        Serial.println();
    };

    if (observationCount == 0) {
        printInspectCore(nullptr, detection::InspectionTarget::None);
        return;
    }

    for (size_t i = 0; i < observationCount; ++i) {
        printInspectCore(&report.profileDetail.inspectionObservations[i],
                         report.profileDetail.inspectionObservationTargets[i]);
    }
}

void AnalyzerApp::printSequenceSourceCanonical(const AnalyzerReport& report) const {
    if (!shouldPrintSequenceSource(report)) {
        return;
    }

    Serial.print("SEQ_SOURCE");
    Serial.print(" source.selection=");
    Serial.print(report.sourceSelection != nullptr ? report.sourceSelection : "none");
    Serial.print(" source.occurrence_id=");
    Serial.print(report.sourceOccurrenceId);
    Serial.print(" source.candidate_id=");
    Serial.print(report.sourceCandidateId);
    Serial.print(" source.report_matched=");
    Serial.print(report.sourceReportMatched ? 1 : 0);
    Serial.print(" source.report_reason=");
    Serial.print(report.sourceReportReason != nullptr ? report.sourceReportReason : "none");
    Serial.print(" source.kind=");
    Serial.print(report.occurrences.kind != nullptr ? report.occurrences.kind : "none");
    Serial.print(" source.source=");
    Serial.print(report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "none");
    Serial.print(" source.present=");
    Serial.print(report.occurrences.present ? 1 : 0);
    Serial.print(" source.valid=");
    Serial.print(report.occurrences.valid ? 1 : 0);
    Serial.print(" source.start_ms=");
    Serial.print(report.occurrences.startMs);
    Serial.print(" source.peak_ms=");
    Serial.print(report.occurrences.peakMs);
    Serial.print(" source.release_ms=");
    Serial.print(report.occurrences.releaseMs);
    Serial.print(" source.duration_ms=");
    Serial.print(report.occurrences.primaryDurationMs);
    Serial.print(" source.strength=");
    Serial.print(report.occurrences.primaryStrength, 1);
    Serial.print(" source.confidence=");
    Serial.print(report.occurrences.confidence, 2);
    Serial.print(" source.reject_reason=");
    Serial.print(report.occurrences.rejectReason != nullptr ? report.occurrences.rejectReason : "none");
    Serial.print(" source.total=");
    Serial.print(report.occurrences.total);
    Serial.print(" source.accepted_count=");
    Serial.print(report.occurrences.accepted);
    Serial.print(" source.rejected_count=");
    Serial.print(report.occurrences.rejected);
    Serial.print(" integrity.complete=");
    Serial.print(report.integrity.correlationComplete ? 1 : 0);
    Serial.print(" integrity.queue_overflow=");
    Serial.print(report.integrity.queueOverflowAffected ? 1 : 0);
    Serial.print(" integrity.reason=");
    Serial.print(report.integrity.reason != nullptr ? report.integrity.reason : "none");
    Serial.println();

    printSequenceSourceCoreCanonical(report);
    printSequenceSourceSpecCanonical(report);
}

void AnalyzerApp::printSequenceSourceCoreCanonical(const AnalyzerReport& report) const {
    detection::printDetectorReportGenericLine("SEQ_SOURCE_CORE", report.context.trial, report.detectorReport);
}

void AnalyzerApp::printSequenceSourceSpecCanonical(const AnalyzerReport& report) const {
    detection::printDetectorDetailLine("SEQ_SOURCE_SPEC", report.detectorReport);
}

void AnalyzerApp::printSequenceDetailCanonical(const AnalyzerReport& report) const {
    if (!shouldPrintSequenceDetail(report)) {
        return;
    }

    Serial.print("SEQ_EXPLAIN");
    Serial.print(" trial=");
    Serial.print(report.context.trial);
    Serial.print(" profile=");
    Serial.print(report.context.profile != nullptr ? report.context.profile : "unknown");
    Serial.print(" source.selection=");
    Serial.print(report.sourceSelection != nullptr ? report.sourceSelection : "none");
    Serial.print(" source.occurrence_id=");
    Serial.print(report.sourceOccurrenceId);
    Serial.print(" source.report_matched=");
    Serial.print(report.sourceReportMatched ? 1 : 0);
    Serial.print(" DETECTOR:");
    Serial.print(" lifecycle=");
    Serial.print(report.detectorReport != nullptr && report.detectorReport->accepted.present ? "valid" : "invalid");
    Serial.print(" carrier_quality=");
    if (report.detectorReport != nullptr) {
        const auto& detector = *report.detectorReport;
        const bool carrierQualityPass =
            detector.scalar.inspect.carrierQualityRequired
                ? (detector.scalar.inspect.carrierCoveragePassed &&
                   detector.scalar.inspect.carrierIslandPassed &&
                   detector.scalar.inspect.carrierGapPassed)
                : true;
        Serial.print(carrierQualityPass ? "valid" : "invalid");
        Serial.print(" carrier_quality_required=");
        Serial.print(detector.scalar.inspect.carrierQualityRequired ? 1 : 0);
        Serial.print(" carrier_coverage_passed=");
        Serial.print(detector.scalar.inspect.carrierCoveragePassed ? 1 : 0);
        Serial.print(" carrier_island_passed=");
        Serial.print(detector.scalar.inspect.carrierIslandPassed ? 1 : 0);
        Serial.print(" carrier_gap_passed=");
        Serial.print(detector.scalar.inspect.carrierGapPassed ? 1 : 0);
    } else {
        Serial.print("unknown");
    }
    Serial.print(" INSPECT:");
    Serial.print(" contrast.class=");
    Serial.print(strengthClassForObservationTarget(report, detection::InspectionTarget::Contrast));
    Serial.print(" amp.class=");
    Serial.print(strengthClassForObservationTarget(report, detection::InspectionTarget::Amp));
    Serial.print(" coverage.complete=");
    Serial.print(report.profileDetail.scalarObservation.coverageComplete ? 1 : 0);
    Serial.print(" coverage.future_unavailable=");
    Serial.print(report.profileDetail.scalarObservation.requestedFutureAtInspection ? 1 : 0);
    Serial.print(" coverage.covered_ms=");
    Serial.print(report.profileDetail.scalarObservation.coveredDurationMs);
    Serial.print(" integrity.detector_present=");
    Serial.print(report.integrity.detectorReportPresent ? 1 : 0);
    Serial.print(" integrity.occurrence_matched=");
    Serial.print(report.integrity.occurrenceMatched ? 1 : 0);
    Serial.print(" integrity.inspection_present=");
    Serial.print(report.integrity.inspectionPresent ? 1 : 0);
    Serial.print(" integrity.pattern_report_present=");
    Serial.print(report.integrity.patternReportPresent ? 1 : 0);
    Serial.print(" integrity.pattern_result_present=");
    Serial.print(report.integrity.patternResultPresent ? 1 : 0);
    Serial.print(" integrity.complete=");
    Serial.print(report.integrity.correlationComplete ? 1 : 0);
    Serial.print(" integrity.queue_overflow=");
    Serial.print(report.integrity.queueOverflowAffected ? 1 : 0);
    Serial.print(" integrity.reason=");
    Serial.print(report.integrity.reason != nullptr ? report.integrity.reason : "none");
    Serial.print(" PATTERN:");
    Serial.print(" result=");
    Serial.print(report.classification.result == AnalyzerResult::Expected || report.classification.result == AnalyzerResult::Late
        ? "confirmed"
        : "rejected");
    Serial.print(" pattern.first_failed_requirement_index=");
    Serial.print(report.primaryPattern.firstFailedRequirementIndex);
    Serial.print(" pattern.first_failed_label=");
    Serial.print(detection::inspectionTargetName(report.primaryPattern.firstFailedTarget));
    Serial.print(" pattern.observed_class=");
    Serial.print(report.primaryPattern.firstFailedObservedStrength != nullptr ? report.primaryPattern.firstFailedObservedStrength : "unknown");
    Serial.print(" pattern.required_class=");
    Serial.print(report.primaryPattern.firstFailedRequiredStrength != nullptr ? report.primaryPattern.firstFailedRequiredStrength : "unknown");
    Serial.println();
}

void AnalyzerApp::printSequenceSummaryClean() const {
    const auto& summary = _sequenceTest.cleanSummary;
    const long avgDtRounded = summary.dtCount > 0
        ? static_cast<long>(static_cast<float>(summary.totalDtMs) / static_cast<float>(summary.dtCount) + 0.5f)
        : -1L;
    const float avgStrength = summary.strengthCount > 0
        ? summary.totalStrength / static_cast<float>(summary.strengthCount)
        : 0.0f;
    const float avgConfidence = summary.confidenceCount > 0
        ? summary.totalConfidence / static_cast<float>(summary.confidenceCount)
        : 0.0f;

    Serial.print("SEQ_SUMMARY profile=");
    Serial.print(summary.profileName != nullptr ? summary.profileName : "unknown");
    Serial.print(" detector=");
    Serial.print(detection::detectorIdName(summary.detectorId));
    Serial.print(" trials=");
    Serial.print(summary.trials);
    Serial.print(" completed=");
    Serial.print(summary.completed);
    Serial.print(" expected_trials=");
    Serial.print(summary.expectedTrials);
    Serial.print(" early_trials=");
    Serial.print(summary.earlyTrials);
    Serial.print(" late_trials=");
    Serial.print(summary.lateTrials);
    Serial.print(" miss_trials=");
    Serial.print(summary.missTrials);
    Serial.print(" duplicate_trials=");
    Serial.print(summary.duplicateTrials);
    Serial.print(" unexpected_trials=");
    Serial.print(summary.unexpectedTrials);
    Serial.print(" rejected_trials=");
    Serial.print(summary.rejectedTrials);
    Serial.print(" buffer_overrun_trials=");
    Serial.print(summary.bufferOverrunTrials);
    Serial.print(" ambiguous_trials=");
    Serial.print(summary.ambiguousTrials);
    Serial.print(" too_dense_trials=");
    Serial.print(summary.tooDenseTrials);
    Serial.print(" detector_accepted_trials=");
    Serial.print(summary.detectorAcceptedTrials);
    Serial.print(" detector_reject_trials=");
    Serial.print(summary.detectorSelectedRejectTrials);
    Serial.print(" pattern_valid_trials=");
    Serial.print(summary.patternValidTrials);
    Serial.print(" pattern_rejected_trials=");
    Serial.print(summary.patternRejectedTrials);
    Serial.print(" avg_dt_ms=");
    if (avgDtRounded >= 0L) {
        Serial.print(avgDtRounded);
    } else {
        Serial.print(-1);
    }
    Serial.print(" avg_strength=");
    Serial.print(avgStrength, 1);
    Serial.print(" avg_conf=");
    Serial.println(avgConfidence, 2);
}

void AnalyzerApp::printSequenceReport() const {
    if (_sequenceTest.showDetails) {
        printDetectionParameters();
    }
    if (_sequenceTest.outputConfig.verbosity > 0U ||
        _sequenceTest.outputConfig.mode == SeqOutputMode::Detail) {
        printAudioSourceSummary();
        printOccurrenceSummary();
    }
    if (_sequenceTest.outputConfig.verbosity > 0U ||
        _sequenceTest.outputConfig.mode == SeqOutputMode::System ||
        _sequenceTest.outputConfig.mode == SeqOutputMode::Detail) {
        printAudioRunSummary();
    }
}

void AnalyzerApp::printSequenceStatus() const {
    Serial.print("SEQ_STATUS mode=");
    Serial.print(sequenceOutputModeName(_sequenceTest.outputConfig.mode));
    Serial.print(" when=");
    Serial.print(sequenceOutputWhenName(_sequenceTest.outputConfig.when));
    Serial.print(" verbosity=");
    Serial.print(_sequenceTest.outputConfig.verbosity);
    Serial.print(" profile=");
    Serial.print(_sequenceTest.active ? activeAnalyzerProfileName() : detection::detectionProfileName(_seqOutputConfig.profileKind));
    Serial.print(" tries=");
    Serial.print(_seqOutputConfig.totalTrials);
    Serial.print(" diagnostics=");
    Serial.print(_seqOutputConfig.diagnosticsEnabled ? "on" : "off");
    Serial.print(" freqband=");
    Serial.print(_seqOutputConfig.frequencyBandEnabled ? "on" : "off");
    Serial.print(" freqUpdateEverySamples=");
    Serial.print(_seqOutputConfig.frequencyUpdateEverySamples);
    Serial.println();
}
