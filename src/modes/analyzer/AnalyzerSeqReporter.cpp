#include "AnalyzerModeApp.h"

#include <Arduino.h>
#include <string.h>

#include "../../detection/detectors/DetectorNames.h"
#include "report/DetectorReportPrinter.h"
#include "../../detection/inspection/InspectionNames.h"
#include "../../app/TimeBase.h"
#include "../../app/SerialOut.h"

#include "../../app/PrintKeyValue.h"
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
    printKeyValue("expected.start_ms", report.expected.windowStartMs);
    printKeyValue("expected.end_ms", report.expected.windowEndMs);
    printKeyValue("verdict.type", report.primaryPattern.type != nullptr ? report.primaryPattern.type : "unknown");
    printKeyValue("verdict.valid", report.primaryPattern.valid ? 1 : 0);
    printKeyValue("verdict.reason", report.primaryPattern.reason != nullptr ? report.primaryPattern.reason : "none");
    printKeyValue("analyzer.result", analyzerResultName(report.classification.result));
    printKeyValue("analyzer.stage", analyzerStageName(report.classification.primaryStage));
    printKeyValue("analyzer.reason", analyzerReasonName(report.classification.reason));
    printKeyValue("analyzer.dt_ms", report.classification.dtMs);
    if (extended) {
        printKeyValue("occurrence.present", report.occurrences.present ? 1 : 0);
        printKeyValue("occurrence.source",
                      report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "none");
        printKeyValue("inspection.label", detection::inspectionTargetName(report.inspection.moduleTarget));
        printKeyValue("inspection.strength",
                      report.inspection.moduleStrengthClass != nullptr ? report.inspection.moduleStrengthClass
                                                                       : "unknown");
        printKeyValue("integrity.complete", report.integrity.correlationComplete ? 1 : 0);
        printKeyValue("integrity.reason", report.integrity.reason != nullptr ? report.integrity.reason : "none");
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
    printKeyValue("result", analyzerResultName(report.classification.result));
    printKeyValue("stage", analyzerStageName(report.classification.primaryStage));
    printKeyValue("reject_reason", trialRejectReason);
    serialOut().print(" emit_marker_seen=");  // moved from SEQ_HISTDBG (11g C3b)
    serialOut().print(_sequenceTest.trial.diagnostics.emitStartSeen ? 1 : 0);
    printKeyValue("source_selection", report.sourceSelection != nullptr ? report.sourceSelection : "none");
    printKeyValue("source_reason", report.sourceReportReason != nullptr ? report.sourceReportReason : "none");
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
        printKeyValue("duration_ms", report.occurrences.primaryDurationMs);
    } else if (report.detectorReport != nullptr && report.detectorReport->accepted.present) {
        printKeyValue("duration_ms", report.detectorReport->accepted.durationMs);
    }
    printKeyValue("contrast_class", strengthClassForObservationTarget(report, detection::InspectionTarget::Contrast));
    printKeyValue("amp_class", strengthClassForObservationTarget(report, detection::InspectionTarget::Amp));
    if (report.detectorReport != nullptr) {
        const auto& accepted = report.detectorReport->accepted;
        const auto& selectedReject = report.detectorReport->selectedReject;
        const bool useAccepted = accepted.present;
        const auto coverageMs = useAccepted ? accepted.coverageAboveReleaseMs : selectedReject.coverageAboveReleaseMs;
        const auto islandMaxMs = useAccepted ? accepted.islandMaxMs : selectedReject.islandMaxMs;
        const auto gapMaxMs = useAccepted ? accepted.gapMaxMs : selectedReject.gapMaxMs;
        printKeyValue("coverage_ms", coverageMs);
        printKeyValue("island_max_ms", islandMaxMs);
        printKeyValue("gap_max_ms", gapMaxMs);
    }
    printKeyValue("source_strength", report.occurrences.primaryStrength, 1);
    printKeyValue("dup", report.debug.duplicates);
    printKeyValue("src_total", report.occurrences.total);
    printKeyValue("src_acc", report.occurrences.accepted);
    printKeyValue("src_rej", report.occurrences.rejected);
    printKeyValue("buffer_overrun", report.debug.bufferOverrun ? 1 : 0);
    printKeyValue("profile", report.context.profile != nullptr ? report.context.profile : "unknown");
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
        printKeyValue("inspect.label", detection::inspectionTargetName(target));
        printKeyValue("inspect.anchor_ms", observation != nullptr ? observation->anchorMs : 0UL);
        printKeyValue("inspect.requested_start_ms", observation != nullptr ? observation->requestedStartMs : 0UL);
        printKeyValue("inspect.requested_end_ms", observation != nullptr ? observation->requestedEndMs : 0UL);
        printKeyValue("inspect.inspection_now_ms", observation != nullptr ? observation->inspectionNowMs : 0UL);
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
        printKeyValue("inspect.value", available ? observation->classificationValue : 0.0f, 3);
        printKeyValue("inspect.strength_class",
                      available ? detection::strengthClassName(observation->strength) : "unknown");
        printKeyValue("inspect.valid", available ? 1 : 0);
        printKeyValue("inspect.status", available ? "observed" : "missing");
        printKeyValue("inspect.reject_reason",
                      available ? "none"
                                : detection::magnitudeInspectionNoteName(
                                      observation != nullptr ? observation->note
                                                             : detection::MagnitudeInspectionNote::WindowInvalid));
        printKeyValue("inspect.stream",
                      observation != nullptr ? scalarObservedStreamDisplayName(observation->stream) : "unknown");
        printKeyValue("inspect.metric",
                      observation != nullptr ? detection::magnitudeInspectionModeName(observation->mode) : "unknown");
        printKeyValue("inspect.window_ms", observation != nullptr ? observation->windowMs : 0UL);
        printKeyValue("inspect.expected_bin_count", observation != nullptr ? observation->windowMs : 0UL);
        printKeyValue("inspect.sample_count",
                      observation != nullptr ? static_cast<unsigned long>(observation->sampleCount) : 0UL);
        printKeyValue("inspect.coverage", observation != nullptr ? observation->coverageRatio : 0.0f, 3);
        printKeyValue("inspect.covered_ms", observation != nullptr ? observation->coveredDurationMs : 0UL);
        printKeyValue("inspect.coverage_complete", observation != nullptr && observation->coverageComplete ? 1 : 0);
        printKeyValue("inspect.future_unavailable",
                      observation != nullptr && observation->requestedFutureAtInspection ? 1 : 0);
        printKeyValue("inspect.peak", observation != nullptr ? observation->peak : 0.0f, 3);
        printKeyValue("inspect.mean", observation != nullptr ? observation->mean : 0.0f, 3);
        printKeyValue("inspect.rms", observation != nullptr ? observation->rms : 0.0f, 3);
        printKeyValue("inspect.median", observation != nullptr ? observation->median : 0.0f, 3);
        printKeyValue("inspect.p75", observation != nullptr ? observation->p75 : 0.0f, 3);
        printKeyValue("inspect.p90", observation != nullptr ? observation->p90 : 0.0f, 3);
        printKeyValue("inspect.trimmed_mean", observation != nullptr ? observation->trimmedMean : 0.0f, 3);
        printKeyValue("inspect.input_value_count",
                      observation != nullptr ? static_cast<unsigned long>(observation->freshValueCount) : 0UL);
        printKeyValue("inspect.occurrence_id", report.sourceOccurrenceId);
        printKeyValue("inspect.integrity_complete", report.integrity.correlationComplete ? 1 : 0);
        printKeyValue("inspect.integrity_reason",
                      report.integrity.reason != nullptr ? report.integrity.reason : "none");
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
    printKeyValue("trial", report.context.trial);
    printKeyValue("profile", report.context.profile != nullptr ? report.context.profile : "unknown");
    printKeyValue("source.selection", report.sourceSelection != nullptr ? report.sourceSelection : "none");
    printKeyValue("source.occurrence_id", report.sourceOccurrenceId);
    printKeyValue("source.event_id", report.sourceEventId);
    printKeyValue("source.report_generation", report.sourceReportGeneration);
    printKeyValue("source.event_trial_attribution", report.sourceEventTrialAttribution);
    printKeyValue("source.report_matched", report.sourceReportMatched ? 1 : 0);
    serialOut().print(" DETECTOR:");
    printKeyValue("lifecycle",
                  report.detectorReport != nullptr && report.detectorReport->accepted.present ? "valid" : "invalid");
    serialOut().print(" carrier_quality=");
    if (report.detectorReport != nullptr) {
        const auto& detector = *report.detectorReport;
        const bool carrierQualityPass = detector.scalar.inspect.carrierQualityRequired
            ? (detector.scalar.inspect.carrierCoveragePassed && detector.scalar.inspect.carrierIslandPassed &&
               detector.scalar.inspect.carrierGapPassed)
            : true;
        serialOut().print(carrierQualityPass ? "valid" : "invalid");
        printKeyValue("carrier_quality_required", detector.scalar.inspect.carrierQualityRequired ? 1 : 0);
        printKeyValue("carrier_coverage_passed", detector.scalar.inspect.carrierCoveragePassed ? 1 : 0);
        printKeyValue("carrier_island_passed", detector.scalar.inspect.carrierIslandPassed ? 1 : 0);
        printKeyValue("carrier_gap_passed", detector.scalar.inspect.carrierGapPassed ? 1 : 0);
    } else {
        serialOut().print("unknown");
    }
    serialOut().print(" INSPECT:");
    printKeyValue("contrast.class", strengthClassForObservationTarget(report, detection::InspectionTarget::Contrast));
    printKeyValue("amp.class", strengthClassForObservationTarget(report, detection::InspectionTarget::Amp));
    printKeyValue("coverage.complete", report.profileDetail.magnitudeObservation.coverageComplete ? 1 : 0);
    printKeyValue("coverage.covered_ms", report.profileDetail.magnitudeObservation.coveredDurationMs);
    printKeyValue("integrity.detector_present", report.integrity.detectorReportPresent ? 1 : 0);
    printKeyValue("integrity.occurrence_matched", report.integrity.occurrenceMatched ? 1 : 0);
    printKeyValue("integrity.inspection_present", report.integrity.inspectionPresent ? 1 : 0);
    printKeyValue("integrity.verdict_present", report.integrity.verdictPresent ? 1 : 0);
    printKeyValue("integrity.complete", report.integrity.correlationComplete ? 1 : 0);
    printKeyValue("integrity.reason", report.integrity.reason != nullptr ? report.integrity.reason : "none");
    serialOut().print(" PATTERN:");
    printKeyValue("result",
                  report.classification.result == AnalyzerResult::Expected ||
                          report.classification.result == AnalyzerResult::Late
                      ? "confirmed"
                      : "rejected");
    printKeyValue("verdict.first_failed_requirement_index", report.primaryPattern.firstFailedRequirementIndex);
    printKeyValue("verdict.first_failed_label",
                  detection::inspectionTargetName(report.primaryPattern.firstFailedTarget));
    printKeyValue("verdict.observed_class",
                  report.primaryPattern.firstFailedObservedStrength != nullptr
                      ? report.primaryPattern.firstFailedObservedStrength
                      : "unknown");
    printKeyValue("verdict.required_class",
                  report.primaryPattern.firstFailedRequiredStrength != nullptr
                      ? report.primaryPattern.firstFailedRequiredStrength
                      : "unknown");
    // The inspected occurrence's shape (DET-015, step 13.0): bins visited, steps up, other steps, runs, the first and
    // last bin, the class; applicable=0 without band bins or an inspected occurrence.
    const detection::OccurrenceShape& shape = report.occurrences.shape;
    printKeyValue("shape.applicable", shape.applicable ? 1 : 0);
    printKeyValue("shape.distinct", static_cast<unsigned>(shape.distinctBins));
    printKeyValue("shape.up", static_cast<unsigned>(shape.risingSteps));
    printKeyValue("shape.other", static_cast<unsigned>(shape.otherSteps));
    printKeyValue("shape.runs", static_cast<unsigned>(shape.runs));
    printKeyValue("shape.first_hz", static_cast<unsigned>(shape.firstBinHz));
    printKeyValue("shape.last_hz", static_cast<unsigned>(shape.lastBinHz));
    printKeyValue("shape.class", detection::strengthClassName(shape.shapeClass));
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
    printKeyValue("detector", detection::detectorIdName(summary.detectorId));
    printKeyValue("trials", summary.trials);
    printKeyValue("completed", summary.completed);
    printKeyValue("expected_trials", summary.expectedTrials);
    printKeyValue("early_trials", summary.earlyTrials);
    printKeyValue("late_trials", summary.lateTrials);
    printKeyValue("miss_trials", summary.missTrials);
    printKeyValue("duplicate_trials", summary.duplicateTrials);
    printKeyValue("unexpected_trials", summary.unexpectedTrials);
    printKeyValue("rejected_trials", summary.rejectedTrials);
    printKeyValue("buffer_overrun_trials", summary.bufferOverrunTrials);
    printKeyValue("detector_accepted_trials", summary.detectorAcceptedTrials);
    printKeyValue("detector_reject_trials", summary.detectorSelectedRejectTrials);
    printKeyValue("pattern_valid_trials", summary.patternValidTrials);
    printKeyValue("pattern_rejected_trials", summary.patternRejectedTrials);
    serialOut().print(" avg_dt_ms=");
    if (avgDtRounded >= 0L) {
        serialOut().print(avgDtRounded);
    } else {
        serialOut().print(-1);
    }
    printKeyValue("avg_strength", avgStrength, 1);
    // The run's dropped DMA buffers (cumulative since SEQ start; soak.py reads the last value), moved from SEQ_HISTDBG.
    printKeyValue("dropped_dma_buffers", _audioSource.stats().droppedDmaBuffers);
    serialOut().println();
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
    printKeyValue("when", sequenceOutputWhenName(config.when));
    printKeyValue("verbosity", config.verbosity);
    printKeyValue("profile",
                  _sequenceTest.active ? activeAnalyzerProfileName()
                                       : detection::detectionProfileName(_seqOutputConfig.profileKind));
    printKeyValue("tries", config.totalTrials);
    printKeyValue("diagnostics", config.diagnosticsEnabled ? "on" : "off");
    printKeyValue("freqUpdateEverySamples", config.frequencyUpdateEverySamples);
    serialOut().println();
}
