#include "AnalyzerModeApp.h"

#include <Arduino.h>
#include <string.h>

#include "../../detection/detectors/DetectorNames.h"
#include "report/DetectorReportPrinter.h"
#include "../../detection/inspection/InspectionNames.h"
#include "../../app/TimeBase.h"
#include "../../app/SerialOut.h"

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
        case detection::PipelineIntegrityReason::MissingVerdict:
            return "missing_verdict";
        case detection::PipelineIntegrityReason::InspectionQueueOverflow:
            return "inspection_queue_overflow";
        case detection::PipelineIntegrityReason::VerdictQueueOverflow:
            return "verdict_queue_overflow";
        case detection::PipelineIntegrityReason::PipelineEventQueueOverflow:
            return "pipeline_event_queue_overflow";
        default:
            return "unknown";
    }
}

void printCanonicalStageLine(const char* prefix, const AnalyzerReport& report, bool extended) {
    serialOut().print(prefix);
    serialOut().print(" expected.start_ms=");
    serialOut().print(report.expected.windowStartMs);
    serialOut().print(" expected.end_ms=");
    serialOut().print(report.expected.windowEndMs);
    serialOut().print(" verdict.type=");
    serialOut().print(report.primaryPattern.type != nullptr ? report.primaryPattern.type : "unknown");
    serialOut().print(" verdict.valid=");
    serialOut().print(report.primaryPattern.valid ? 1 : 0);
    serialOut().print(" verdict.reason=");
    serialOut().print(report.primaryPattern.reason != nullptr ? report.primaryPattern.reason : "none");
    serialOut().print(" analyzer.result=");
    serialOut().print(analyzerResultName(report.classification.result));
    serialOut().print(" analyzer.stage=");
    serialOut().print(analyzerStageName(report.classification.primaryStage));
    serialOut().print(" analyzer.reason=");
    serialOut().print(analyzerReasonName(report.classification.reason));
    serialOut().print(" analyzer.dt_ms=");
    serialOut().print(report.classification.dtMs);
    if (extended) {
        serialOut().print(" occurrence.present=");
        serialOut().print(report.occurrences.present ? 1 : 0);
        serialOut().print(" occurrence.source=");
        serialOut().print(report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "none");
        serialOut().print(" inspection.label=");
        serialOut().print(detection::inspectionTargetName(report.inspection.moduleTarget));
        serialOut().print(" inspection.strength=");
        serialOut().print(report.inspection.moduleStrengthClass != nullptr ? report.inspection.moduleStrengthClass
                                                                           : "unknown");
        serialOut().print(" integrity.complete=");
        serialOut().print(report.integrity.correlationComplete ? 1 : 0);
        serialOut().print(" integrity.reason=");
        serialOut().print(report.integrity.reason != nullptr ? report.integrity.reason : "none");
    }
    serialOut().println();
}

}

/*
Canonical analyzer reporting surface.
*/

void AnalyzerApp::printSequenceTrialHeader(unsigned long trialNumber) const {
    serialOut().println();
    serialOut().print("#");
    serialOut().print(trialNumber);
    serialOut().print(" ----------------");
    serialOut().println();
}

void AnalyzerApp::printSequenceTrial(const AnalyzerReport& report) const {
    const char* trialRejectReason = analyzerReasonName(report.classification.reason);
    if (report.classification.result == AnalyzerResult::Rejected && report.occurrences.rejectReason != nullptr &&
        report.occurrences.rejectReason[0] != '\0' && strcmp(report.occurrences.rejectReason, "none") != 0 &&
        strcmp(report.occurrences.rejectReason, trialRejectReason) != 0) {
        trialRejectReason = report.occurrences.rejectReason;
    }

    serialOut().print("SEQ_TRIAL trial=");
    serialOut().print(report.context.trial);
    serialOut().print(" result=");
    serialOut().print(analyzerResultName(report.classification.result));
    serialOut().print(" stage=");
    serialOut().print(analyzerStageName(report.classification.primaryStage));
    serialOut().print(" reject_reason=");
    serialOut().print(trialRejectReason);
    serialOut().print(" emit_marker_seen=");  // moved from SEQ_HISTDBG (11g C3b)
    serialOut().print(_sequenceTest.trial.diagnostics.emitStartSeen ? 1 : 0);
    serialOut().print(" source_selection=");
    serialOut().print(report.sourceSelection != nullptr ? report.sourceSelection : "none");
    serialOut().print(" source_reason=");
    serialOut().print(report.sourceReportReason != nullptr ? report.sourceReportReason : "none");
    serialOut().print(" dt=");
    if (report.classification.result == AnalyzerResult::Rejected) {
        serialOut().print("na");
    } else if (report.classification.dtMs >= 0) {
        serialOut().print(report.classification.dtMs);
        serialOut().print("ms");
    } else {
        serialOut().print(report.classification.dtMs);
        serialOut().print("ms");
    }
    if (report.occurrences.primaryDurationMs > 0) {
        serialOut().print(" duration_ms=");
        serialOut().print(report.occurrences.primaryDurationMs);
    } else if (report.detectorReport != nullptr && report.detectorReport->accepted.present) {
        serialOut().print(" duration_ms=");
        serialOut().print(report.detectorReport->accepted.durationMs);
    }
    serialOut().print(" contrast_class=");
    serialOut().print(strengthClassForObservationTarget(report, detection::InspectionTarget::Contrast));
    serialOut().print(" amp_class=");
    serialOut().print(strengthClassForObservationTarget(report, detection::InspectionTarget::Amp));
    if (report.detectorReport != nullptr) {
        const auto& accepted = report.detectorReport->accepted;
        const auto& selectedReject = report.detectorReport->selectedReject;
        const bool useAccepted = accepted.present;
        const auto coverageMs = useAccepted ? accepted.coverageAboveReleaseMs : selectedReject.coverageAboveReleaseMs;
        const auto islandMaxMs = useAccepted ? accepted.islandMaxMs : selectedReject.islandMaxMs;
        const auto gapMaxMs = useAccepted ? accepted.gapMaxMs : selectedReject.gapMaxMs;
        serialOut().print(" coverage_ms=");
        serialOut().print(coverageMs);
        serialOut().print(" island_max_ms=");
        serialOut().print(islandMaxMs);
        serialOut().print(" gap_max_ms=");
        serialOut().print(gapMaxMs);
    }
    serialOut().print(" source_strength=");
    serialOut().print(report.occurrences.primaryStrength, 1);
    serialOut().print(" dup=");
    serialOut().print(report.debug.duplicates);
    serialOut().print(" src_total=");
    serialOut().print(report.occurrences.total);
    serialOut().print(" src_acc=");
    serialOut().print(report.occurrences.accepted);
    serialOut().print(" src_rej=");
    serialOut().print(report.occurrences.rejected);
    serialOut().print(" buffer_overrun=");
    serialOut().print(report.debug.bufferOverrun ? 1 : 0);
    serialOut().print(" profile=");
    serialOut().print(report.context.profile != nullptr ? report.context.profile : "unknown");
    serialOut().println();
}

void AnalyzerApp::printSequenceInspectCanonical(const AnalyzerReport& report) const {
    if (!shouldPrintSequenceInspect(report)) {
        return;
    }

    const size_t observationCount = report.profileDetail.inspectionObservationCount;

    auto printInspectCore = [&report](const detection::MagnitudeInspectionObservation* observation,
                                      detection::InspectionTarget target) {
        const bool available = observation != nullptr && observation->available;
        serialOut().print("SEQ_INSPECT");
        serialOut().print(" inspect.label=");
        serialOut().print(detection::inspectionTargetName(target));
        serialOut().print(" inspect.anchor_ms=");
        serialOut().print(observation != nullptr ? observation->anchorMs : 0UL);
        serialOut().print(" inspect.requested_start_ms=");
        serialOut().print(observation != nullptr ? observation->requestedStartMs : 0UL);
        serialOut().print(" inspect.requested_end_ms=");
        serialOut().print(observation != nullptr ? observation->requestedEndMs : 0UL);
        serialOut().print(" inspect.inspection_now_ms=");
        serialOut().print(observation != nullptr ? observation->inspectionNowMs : 0UL);
        // ANA-009 (step 7a): the history fills the available range only from
        // values inside the requested window; with none (future window,
        // missing stream, empty edge) it stayed 0/0 and read as "empty
        // history". Print na then; inspect.reject_reason keeps the cause.
        const bool rangeKnown = observation != nullptr && observation->hasValues;
        serialOut().print(" inspect.available_start_ms=");
        if (rangeKnown) {
            serialOut().print(observation->availableStartMs);
        } else {
            serialOut().print("na");
        }
        serialOut().print(" inspect.available_end_ms=");
        if (rangeKnown) {
            serialOut().print(observation->availableEndMs);
        } else {
            serialOut().print("na");
        }
        serialOut().print(" inspect.value=");
        serialOut().print(available ? observation->classificationValue : 0.0f, 3);
        serialOut().print(" inspect.strength_class=");
        serialOut().print(available ? detection::strengthClassName(observation->strength) : "unknown");
        serialOut().print(" inspect.valid=");
        serialOut().print(available ? 1 : 0);
        serialOut().print(" inspect.status=");
        serialOut().print(available ? "observed" : "missing");
        serialOut().print(" inspect.reject_reason=");
        serialOut().print(available ? "none"
                                    : detection::magnitudeInspectionNoteName(
                                          observation != nullptr ? observation->note
                                                                 : detection::MagnitudeInspectionNote::WindowInvalid));
        serialOut().print(" inspect.stream=");
        serialOut().print(observation != nullptr ? scalarObservedStreamDisplayName(observation->stream) : "unknown");
        serialOut().print(" inspect.metric=");
        serialOut().print(observation != nullptr ? detection::magnitudeInspectionModeName(observation->mode)
                                                 : "unknown");
        serialOut().print(" inspect.window_ms=");
        serialOut().print(observation != nullptr ? observation->windowMs : 0UL);
        serialOut().print(" inspect.expected_bin_count=");
        serialOut().print(observation != nullptr ? observation->windowMs : 0UL);
        serialOut().print(" inspect.sample_count=");
        serialOut().print(observation != nullptr ? static_cast<unsigned long>(observation->sampleCount) : 0UL);
        serialOut().print(" inspect.coverage=");
        serialOut().print(observation != nullptr ? observation->coverageRatio : 0.0f, 3);
        serialOut().print(" inspect.covered_ms=");
        serialOut().print(observation != nullptr ? observation->coveredDurationMs : 0UL);
        serialOut().print(" inspect.coverage_complete=");
        serialOut().print(observation != nullptr && observation->coverageComplete ? 1 : 0);
        serialOut().print(" inspect.future_unavailable=");
        serialOut().print(observation != nullptr && observation->requestedFutureAtInspection ? 1 : 0);
        serialOut().print(" inspect.peak=");
        serialOut().print(observation != nullptr ? observation->peak : 0.0f, 3);
        serialOut().print(" inspect.mean=");
        serialOut().print(observation != nullptr ? observation->mean : 0.0f, 3);
        serialOut().print(" inspect.rms=");
        serialOut().print(observation != nullptr ? observation->rms : 0.0f, 3);
        serialOut().print(" inspect.median=");
        serialOut().print(observation != nullptr ? observation->median : 0.0f, 3);
        serialOut().print(" inspect.p75=");
        serialOut().print(observation != nullptr ? observation->p75 : 0.0f, 3);
        serialOut().print(" inspect.p90=");
        serialOut().print(observation != nullptr ? observation->p90 : 0.0f, 3);
        serialOut().print(" inspect.trimmed_mean=");
        serialOut().print(observation != nullptr ? observation->trimmedMean : 0.0f, 3);
        serialOut().print(" inspect.input_value_count=");
        serialOut().print(observation != nullptr ? static_cast<unsigned long>(observation->freshValueCount) : 0UL);
        serialOut().print(" inspect.occurrence_id=");
        serialOut().print(report.sourceOccurrenceId);
        serialOut().print(" inspect.integrity_complete=");
        serialOut().print(report.integrity.correlationComplete ? 1 : 0);
        serialOut().print(" inspect.integrity_reason=");
        serialOut().print(report.integrity.reason != nullptr ? report.integrity.reason : "none");
        serialOut().println();
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

    printSequenceSourceCoreCanonical(report);
    printSequenceSourceSpecCanonical(report);
}

void AnalyzerApp::printSequenceSourceCoreCanonical(const AnalyzerReport& report) const {
    const detection::DetectorReport* detectorReport = report.detectorReport;
    detection::DetectorReport correlated = detectorReport != nullptr ? *detectorReport : detection::DetectorReport{};
    correlated.sourceSelection = report.sourceSelection;
    correlated.sourceOccurrenceId = report.sourceOccurrenceId;
    correlated.sourceCandidateId = report.sourceCandidateId;
    correlated.sourceReportMatched = report.sourceReportMatched;
    detection::printDetectorReportGenericLine("SEQ_SOURCE_CORE", report.context.trial, &correlated);
}

void AnalyzerApp::printSequenceSourceSpecCanonical(const AnalyzerReport& report) const {
    detection::printDetectorDetailLine("SEQ_SOURCE_SPEC", report.detectorReport);
}

void AnalyzerApp::printSequenceDetailCanonical(const AnalyzerReport& report) const {
    if (!shouldPrintSequenceDetail(report)) {
        return;
    }

    serialOut().print("SEQ_EXPLAIN");
    serialOut().print(" trial=");
    serialOut().print(report.context.trial);
    serialOut().print(" profile=");
    serialOut().print(report.context.profile != nullptr ? report.context.profile : "unknown");
    serialOut().print(" source.selection=");
    serialOut().print(report.sourceSelection != nullptr ? report.sourceSelection : "none");
    serialOut().print(" source.occurrence_id=");
    serialOut().print(report.sourceOccurrenceId);
    serialOut().print(" source.event_id=");
    serialOut().print(report.sourceEventId);
    serialOut().print(" source.report_generation=");
    serialOut().print(report.sourceReportGeneration);
    serialOut().print(" source.event_trial_attribution=");
    serialOut().print(report.sourceEventTrialAttribution);
    serialOut().print(" source.report_matched=");
    serialOut().print(report.sourceReportMatched ? 1 : 0);
    serialOut().print(" DETECTOR:");
    serialOut().print(" lifecycle=");
    serialOut().print(report.detectorReport != nullptr && report.detectorReport->accepted.present ? "valid"
                                                                                                  : "invalid");
    serialOut().print(" carrier_quality=");
    if (report.detectorReport != nullptr) {
        const auto& detector = *report.detectorReport;
        const bool carrierQualityPass = detector.scalar.inspect.carrierQualityRequired
            ? (detector.scalar.inspect.carrierCoveragePassed && detector.scalar.inspect.carrierIslandPassed &&
               detector.scalar.inspect.carrierGapPassed)
            : true;
        serialOut().print(carrierQualityPass ? "valid" : "invalid");
        serialOut().print(" carrier_quality_required=");
        serialOut().print(detector.scalar.inspect.carrierQualityRequired ? 1 : 0);
        serialOut().print(" carrier_coverage_passed=");
        serialOut().print(detector.scalar.inspect.carrierCoveragePassed ? 1 : 0);
        serialOut().print(" carrier_island_passed=");
        serialOut().print(detector.scalar.inspect.carrierIslandPassed ? 1 : 0);
        serialOut().print(" carrier_gap_passed=");
        serialOut().print(detector.scalar.inspect.carrierGapPassed ? 1 : 0);
    } else {
        serialOut().print("unknown");
    }
    serialOut().print(" INSPECT:");
    serialOut().print(" contrast.class=");
    serialOut().print(strengthClassForObservationTarget(report, detection::InspectionTarget::Contrast));
    serialOut().print(" amp.class=");
    serialOut().print(strengthClassForObservationTarget(report, detection::InspectionTarget::Amp));
    serialOut().print(" coverage.complete=");
    serialOut().print(report.profileDetail.magnitudeObservation.coverageComplete ? 1 : 0);
    serialOut().print(" coverage.covered_ms=");
    serialOut().print(report.profileDetail.magnitudeObservation.coveredDurationMs);
    serialOut().print(" integrity.detector_present=");
    serialOut().print(report.integrity.detectorReportPresent ? 1 : 0);
    serialOut().print(" integrity.occurrence_matched=");
    serialOut().print(report.integrity.occurrenceMatched ? 1 : 0);
    serialOut().print(" integrity.inspection_present=");
    serialOut().print(report.integrity.inspectionPresent ? 1 : 0);
    serialOut().print(" integrity.verdict_present=");
    serialOut().print(report.integrity.verdictPresent ? 1 : 0);
    serialOut().print(" integrity.complete=");
    serialOut().print(report.integrity.correlationComplete ? 1 : 0);
    serialOut().print(" integrity.reason=");
    serialOut().print(report.integrity.reason != nullptr ? report.integrity.reason : "none");
    serialOut().print(" PATTERN:");
    serialOut().print(" result=");
    serialOut().print(report.classification.result == AnalyzerResult::Expected ||
                              report.classification.result == AnalyzerResult::Late
                          ? "confirmed"
                          : "rejected");
    serialOut().print(" verdict.first_failed_requirement_index=");
    serialOut().print(report.primaryPattern.firstFailedRequirementIndex);
    serialOut().print(" verdict.first_failed_label=");
    serialOut().print(detection::inspectionTargetName(report.primaryPattern.firstFailedTarget));
    serialOut().print(" verdict.observed_class=");
    serialOut().print(report.primaryPattern.firstFailedObservedStrength != nullptr
                          ? report.primaryPattern.firstFailedObservedStrength
                          : "unknown");
    serialOut().print(" verdict.required_class=");
    serialOut().print(report.primaryPattern.firstFailedRequiredStrength != nullptr
                          ? report.primaryPattern.firstFailedRequiredStrength
                          : "unknown");
    serialOut().println();
}

void AnalyzerApp::printSequenceSummaryClean() const {
    const auto& summary = _sequenceTest.totals.cleanSummary;
    const long avgDtRounded = summary.dtCount > 0
        ? static_cast<long>(static_cast<float>(summary.totalDtMs) / static_cast<float>(summary.dtCount) + 0.5f)
        : -1L;
    const float avgStrength =
        summary.strengthCount > 0 ? summary.totalStrength / static_cast<float>(summary.strengthCount) : 0.0f;

    serialOut().print("SEQ_SUMMARY profile=");
    serialOut().print(summary.profileName != nullptr ? summary.profileName : "unknown");
    serialOut().print(" detector=");
    serialOut().print(detection::detectorIdName(summary.detectorId));
    serialOut().print(" trials=");
    serialOut().print(summary.trials);
    serialOut().print(" completed=");
    serialOut().print(summary.completed);
    serialOut().print(" expected_trials=");
    serialOut().print(summary.expectedTrials);
    serialOut().print(" early_trials=");
    serialOut().print(summary.earlyTrials);
    serialOut().print(" late_trials=");
    serialOut().print(summary.lateTrials);
    serialOut().print(" miss_trials=");
    serialOut().print(summary.missTrials);
    serialOut().print(" duplicate_trials=");
    serialOut().print(summary.duplicateTrials);
    serialOut().print(" unexpected_trials=");
    serialOut().print(summary.unexpectedTrials);
    serialOut().print(" rejected_trials=");
    serialOut().print(summary.rejectedTrials);
    serialOut().print(" buffer_overrun_trials=");
    serialOut().print(summary.bufferOverrunTrials);
    serialOut().print(" detector_accepted_trials=");
    serialOut().print(summary.detectorAcceptedTrials);
    serialOut().print(" detector_reject_trials=");
    serialOut().print(summary.detectorSelectedRejectTrials);
    serialOut().print(" pattern_valid_trials=");
    serialOut().print(summary.patternValidTrials);
    serialOut().print(" pattern_rejected_trials=");
    serialOut().print(summary.patternRejectedTrials);
    serialOut().print(" avg_dt_ms=");
    if (avgDtRounded >= 0L) {
        serialOut().print(avgDtRounded);
    } else {
        serialOut().print(-1);
    }
    serialOut().print(" avg_strength=");
    serialOut().print(avgStrength, 1);
    // The run's dropped DMA buffers (cumulative since SEQ start; soak.py reads the last value), moved from SEQ_HISTDBG.
    serialOut().print(" dropped_dma_buffers=");
    serialOut().println(_audioSource.stats().droppedDmaBuffers);
}

void AnalyzerApp::printSequenceReport() const {
    if (_sequenceTest.config.showDetails) {
        printDetectionParameters();
    }
    if (_sequenceTest.config.outputConfig.verbosity > 0U ||
        _sequenceTest.config.outputConfig.mode == SeqOutputMode::Detail) {
        printAudioSourceSummary();
        printOccurrenceSummary();
    }
    if (_sequenceTest.config.outputConfig.verbosity > 0U ||
        _sequenceTest.config.outputConfig.mode == SeqOutputMode::System ||
        _sequenceTest.config.outputConfig.mode == SeqOutputMode::Detail) {
        printAudioRunSummary();
    }
}

void AnalyzerApp::printSequenceStatus() const {
    // One config (review D1.4): the running one during a run, the one the next start copies otherwise.
    const SeqOutputConfig& config = _sequenceTest.active ? _sequenceTest.config.outputConfig : _seqOutputConfig;
    serialOut().print("SEQ_STATUS mode=");
    serialOut().print(sequenceOutputModeName(config.mode));
    serialOut().print(" when=");
    serialOut().print(sequenceOutputWhenName(config.when));
    serialOut().print(" verbosity=");
    serialOut().print(config.verbosity);
    serialOut().print(" profile=");
    serialOut().print(_sequenceTest.active ? activeAnalyzerProfileName()
                                           : detection::detectionProfileName(_seqOutputConfig.profileKind));
    serialOut().print(" tries=");
    serialOut().print(config.totalTrials);
    serialOut().print(" diagnostics=");
    serialOut().print(config.diagnosticsEnabled ? "on" : "off");
    serialOut().print(" freqUpdateEverySamples=");
    serialOut().print(config.frequencyUpdateEverySamples);
    serialOut().println();
}
