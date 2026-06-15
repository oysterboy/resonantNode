#include "../../modes/analyzer/AnalyzerModeApp.h"

#include <Arduino.h>

namespace {

const char* cleanDetectorIdName(detection::DetectorId detectorId) {
    switch (detectorId) {
        case detection::DetectorId::ScalarTransient:
            return "scalar_transient";
        case detection::DetectorId::FrequencyMatch:
            return "frequency_match";
        case detection::DetectorId::Unknown:
        default:
            return "unknown";
    }
}

const char* cleanDetectorRejectClassName(detection::DetectorRejectClass rejectClass) {
    switch (rejectClass) {
        case detection::DetectorRejectClass::None:
            return "none";
        case detection::DetectorRejectClass::Threshold:
            return "threshold";
        case detection::DetectorRejectClass::Timing:
            return "timing";
        case detection::DetectorRejectClass::Strength:
            return "strength";
        case detection::DetectorRejectClass::Cooldown:
            return "cooldown";
        case detection::DetectorRejectClass::State:
            return "state";
        case detection::DetectorRejectClass::Window:
            return "window";
        case detection::DetectorRejectClass::Unknown:
        default:
            return "unknown";
    }
}

void printCanonicalDetectorDetailLine(const char* prefix, const AnalyzerReport& report) {
    if (report.detectorReport == nullptr) {
        return;
    }

    const auto& detectorReport = *report.detectorReport;
    switch (detectorReport.detectorId) {
        case detection::DetectorId::ScalarTransient: {
            const auto& scalar = detectorReport.scalar;
            Serial.print(prefix);
            Serial.print(" detail.scalar.accepted.value=");
            Serial.print(scalar.accepted.value, 1);
            Serial.print(" detail.scalar.accepted.baseline=");
            Serial.print(scalar.accepted.baseline, 1);
            Serial.print(" detail.scalar.accepted.lift=");
            Serial.print(scalar.accepted.lift, 1);
            Serial.print(" detail.scalar.accepted.normalized=");
            Serial.print(scalar.accepted.normalized, 2);
            Serial.print(" detail.scalar.reject.value=");
            Serial.print(scalar.selectedReject.value, 1);
            Serial.print(" detail.scalar.reject.baseline=");
            Serial.print(scalar.selectedReject.baseline, 1);
            Serial.print(" detail.scalar.reject.lift=");
            Serial.print(scalar.selectedReject.lift, 1);
            Serial.print(" detail.scalar.reject.normalized=");
            Serial.print(scalar.selectedReject.normalized, 2);
            Serial.print(" detail.scalar.reject.opened=");
            Serial.print(scalar.selectedReject.opened ? 1 : 0);
            Serial.print(" detail.scalar.reject.crossed_onset=");
            Serial.print(scalar.selectedReject.crossedOnset ? 1 : 0);
            Serial.print(" detail.scalar.reject.crossed_release=");
            Serial.print(scalar.selectedReject.crossedRelease ? 1 : 0);
            Serial.print(" detail.scalar.threshold.onset=");
            Serial.print(scalar.thresholds.onsetThreshold, 1);
            Serial.print(" detail.scalar.threshold.release=");
            Serial.print(scalar.thresholds.releaseThreshold, 1);
            Serial.print(" detail.scalar.threshold.min_strength=");
            Serial.print(scalar.thresholds.minStrength, 1);
            Serial.print(" detail.scalar.aggregate.too_short=");
            Serial.print(scalar.aggregates.tooShortCount);
            Serial.print(" detail.scalar.aggregate.too_long=");
            Serial.print(scalar.aggregates.tooLongCount);
            Serial.print(" detail.scalar.aggregate.strength_too_low=");
            Serial.print(scalar.aggregates.strengthTooLowCount);
            Serial.print(" detail.scalar.aggregate.max_rejected_lift=");
            Serial.print(scalar.aggregates.maxRejectedLift, 1);
            Serial.print(" detail.scalar.aggregate.best_rejected_value=");
            Serial.print(scalar.aggregates.bestRejectedValue, 1);
            Serial.print(" detail.scalar.inspect.reject_reason=");
            Serial.print(scalar.inspect.rejectReason != nullptr ? scalar.inspect.rejectReason : "none");
            Serial.print(" detail.scalar.inspect.no_emit_reason=");
            Serial.print(scalar.inspect.noEmitReason != nullptr ? scalar.inspect.noEmitReason : "none");
            Serial.print(" detail.scalar.inspect.gate_reason=");
            Serial.print(scalar.inspect.gateReason != nullptr ? scalar.inspect.gateReason : "none");
            Serial.print(" detail.scalar.inspect.opened=");
            Serial.print(scalar.inspect.opened ? 1 : 0);
            Serial.print(" detail.scalar.inspect.released=");
            Serial.print(scalar.inspect.released ? 1 : 0);
            Serial.print(" detail.scalar.inspect.valid_release=");
            Serial.print(scalar.inspect.validRelease ? 1 : 0);
            Serial.print(" detail.scalar.inspect.emit_allowed=");
            Serial.print(scalar.inspect.emitAllowed ? 1 : 0);
            Serial.print(" detail.scalar.inspect.open_ms=");
            Serial.print(scalar.inspect.openMs);
            Serial.print(" detail.scalar.inspect.peak_ms=");
            Serial.print(scalar.inspect.peakMs);
            Serial.print(" detail.scalar.inspect.release_ms=");
            Serial.print(scalar.inspect.releaseMs);
            Serial.print(" detail.scalar.inspect.duration_ms=");
            Serial.print(scalar.inspect.durationMs);
            Serial.print(" detail.scalar.inspect.peak_strength=");
            Serial.println(scalar.inspect.peakStrength, 1);
            break;
        }
        case detection::DetectorId::FrequencyMatch: {
            const auto& frequency = detectorReport.frequency;
            Serial.print(prefix);
            Serial.print(" detail.frequency.accepted.score=");
            Serial.print(frequency.accepted.score, 2);
            Serial.print(" detail.frequency.accepted.contrast=");
            Serial.print(frequency.accepted.contrast, 2);
            Serial.print(" detail.frequency.reject.score=");
            Serial.print(frequency.selectedReject.score, 2);
            Serial.print(" detail.frequency.reject.contrast=");
            Serial.print(frequency.selectedReject.contrast, 2);
            Serial.print(" detail.frequency.threshold.score_min=");
            Serial.print(frequency.thresholds.scoreThreshold, 2);
            Serial.print(" detail.frequency.threshold.contrast_min=");
            Serial.print(frequency.thresholds.contrastThreshold, 2);
            Serial.print(" detail.frequency.aggregate.score_ok=");
            Serial.print(frequency.aggregates.scoreOkCount);
            Serial.print(" detail.frequency.aggregate.contrast_ok=");
            Serial.print(frequency.aggregates.contrastOkCount);
            Serial.print(" detail.frequency.aggregate.both_ok=");
            Serial.print(frequency.aggregates.bothOkCount);
            Serial.print(" detail.frequency.aggregate.match=");
            Serial.print(frequency.aggregates.matchCount);
            Serial.print(" detail.frequency.inspect.reject_reason=");
            Serial.print(frequency.inspect.rejectReason != nullptr ? frequency.inspect.rejectReason : "none");
            Serial.print(" detail.frequency.inspect.no_emit_reason=");
            Serial.print(frequency.inspect.noEmitReason != nullptr ? frequency.inspect.noEmitReason : "none");
            Serial.print(" detail.frequency.inspect.gate_reason=");
            Serial.print(frequency.inspect.gateReason != nullptr ? frequency.inspect.gateReason : "none");
            Serial.print(" detail.frequency.inspect.pending_state=");
            Serial.print(frequency.inspect.pendingState != nullptr ? frequency.inspect.pendingState : "none");
            Serial.print(" detail.frequency.inspect.ready_ok=");
            Serial.print(frequency.inspect.readyOk ? 1 : 0);
            Serial.print(" detail.frequency.inspect.gate_open=");
            Serial.print(frequency.inspect.gateOpen ? 1 : 0);
            Serial.print(" detail.frequency.inspect.opened=");
            Serial.print(frequency.inspect.opened ? 1 : 0);
            Serial.print(" detail.frequency.inspect.released=");
            Serial.print(frequency.inspect.released ? 1 : 0);
            Serial.print(" detail.frequency.inspect.emitted=");
            Serial.print(frequency.inspect.emitted ? 1 : 0);
            Serial.print(" detail.frequency.inspect.valid_release=");
            Serial.print(frequency.inspect.validRelease ? 1 : 0);
            Serial.print(" detail.frequency.inspect.emit_allowed=");
            Serial.print(frequency.inspect.emitAllowed ? 1 : 0);
            Serial.print(" detail.frequency.inspect.open_ms=");
            Serial.print(frequency.inspect.openMs);
            Serial.print(" detail.frequency.inspect.peak_ms=");
            Serial.print(frequency.inspect.peakMs);
            Serial.print(" detail.frequency.inspect.release_ms=");
            Serial.print(frequency.inspect.releaseMs);
            Serial.print(" detail.frequency.inspect.duration_ms=");
            Serial.println(frequency.inspect.durationMs);
            break;
        }
        case detection::DetectorId::Unknown:
        default:
            break;
    }
}

void printCanonicalDetectorReportGenericLine(const char* prefix, const AnalyzerReport& report) {
    static const detection::DetectorReport kEmptyDetectorReport = {};
    const auto& detectorReport = report.detectorReport != nullptr ? *report.detectorReport : kEmptyDetectorReport;
    const auto& accepted = detectorReport.accepted;
    const auto& selectedReject = detectorReport.selectedReject;

    Serial.print(prefix);
    Serial.print(" trial=");
    Serial.print(report.context.trial);
    Serial.print(" detector=");
    Serial.print(cleanDetectorIdName(detectorReport.detectorId));
    Serial.print(" window.start_ms=");
    Serial.print(detectorReport.reportStartMs);
    Serial.print(" window.end_ms=");
    Serial.print(detectorReport.reportEndMs);
    Serial.print(" accepted.present=");
    Serial.print(accepted.present ? 1 : 0);
    Serial.print(" accepted.start_ms=");
    Serial.print(accepted.startMs);
    Serial.print(" accepted.peak_ms=");
    Serial.print(accepted.peakMs);
    Serial.print(" accepted.end_ms=");
    Serial.print(accepted.endMs);
    Serial.print(" accepted.duration_ms=");
    Serial.print(accepted.durationMs);
    Serial.print(" accepted.strength=");
    Serial.print(accepted.strength, 1);
    Serial.print(" accepted.confidence=");
    Serial.print(accepted.confidence, 2);
    Serial.print(" reject.present=");
    Serial.print(selectedReject.present ? 1 : 0);
    Serial.print(" reject.class=");
    Serial.print(cleanDetectorRejectClassName(selectedReject.rejectClass));
    Serial.print(" reject.detector_reason=");
    Serial.print(selectedReject.detectorReason != nullptr ? selectedReject.detectorReason : "none");
    Serial.print(" reject.start_ms=");
    Serial.print(selectedReject.startMs);
    Serial.print(" reject.peak_ms=");
    Serial.print(selectedReject.peakMs);
    Serial.print(" reject.end_ms=");
    Serial.print(selectedReject.endMs);
    Serial.print(" reject.duration_ms=");
    Serial.print(selectedReject.durationMs);
    Serial.print(" reject.strength=");
    Serial.print(selectedReject.strength, 1);
    Serial.print(" reject.confidence=");
    Serial.print(selectedReject.confidence, 2);
    Serial.print(" threshold.min_duration_ms=");
    Serial.print(detectorReport.thresholds.minDurationMs);
    Serial.print(" threshold.max_duration_ms=");
    Serial.print(detectorReport.thresholds.maxDurationMs);
    Serial.print(" aggregate.accepted_count=");
    Serial.print(detectorReport.aggregates.acceptedCount);
    Serial.print(" aggregate.rejected_count=");
    Serial.println(detectorReport.aggregates.rejectedCount);
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
    Serial.print(" analyzer.reason=");
    Serial.print(analyzerReasonName(report.classification.reason));
    Serial.print(" analyzer.dt_ms=");
    Serial.print(report.classification.dtMs);
    if (extended) {
        Serial.print(" occurrence.present=");
        Serial.print(report.occurrences.present ? 1 : 0);
        Serial.print(" occurrence.source=");
        Serial.print(report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "none");
        Serial.print(" inspection.target=");
        Serial.print(report.inspection.moduleTarget != nullptr ? report.inspection.moduleTarget : "unknown");
        Serial.print(" inspection.strength=");
        Serial.print(report.inspection.moduleStrengthClass != nullptr ? report.inspection.moduleStrengthClass : "unknown");
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
    Serial.print("SEQ_TRIAL trial=");
    Serial.print(report.context.trial);
    Serial.print(" result=");
    Serial.print(analyzerResultName(report.classification.result));
    if (report.classification.result == AnalyzerResult::Expected ||
        report.classification.result == AnalyzerResult::Late) {
        Serial.print(" dt=");
        if (report.classification.dtMs >= 0) {
            Serial.print(report.classification.dtMs);
            Serial.print("ms");
        } else {
            Serial.print("-1ms");
        }
    }
    if (report.occurrences.primaryDurationMs > 0) {
        Serial.print(" duration_ms=");
        Serial.print(report.occurrences.primaryDurationMs);
    } else if (report.detectorReport != nullptr && report.detectorReport->accepted.present) {
        Serial.print(" duration_ms=");
        Serial.print(report.detectorReport->accepted.durationMs);
    }
    Serial.print(" strength=");
    Serial.print(report.occurrences.primaryStrength, 1);
    Serial.print(" confidence=");
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
    if (report.classification.result != AnalyzerResult::Expected) {
        Serial.print(" reason=");
        Serial.print(analyzerReasonName(report.classification.reason));
    }
    Serial.print(" profile=");
    Serial.print(report.context.profile != nullptr ? report.context.profile : "unknown");
    Serial.println();
}

void AnalyzerApp::printSequenceInspectCanonical(const AnalyzerReport& report) const {
    if (!shouldPrintSequenceInspect(report)) {
        return;
    }

    Serial.print("SEQ_INSPECT");
    Serial.print(" inspect.kind=");
    Serial.print(report.occurrences.kind != nullptr ? report.occurrences.kind : "none");
    Serial.print(" inspect.source=");
    Serial.print(report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "none");
    Serial.print(" inspect.present=");
    Serial.print(report.occurrences.present ? 1 : 0);
    Serial.print(" inspect.valid=");
    Serial.print(report.occurrences.valid ? 1 : 0);
    Serial.print(" inspect.start_ms=");
    Serial.print(report.occurrences.startMs);
    Serial.print(" inspect.peak_ms=");
    Serial.print(report.occurrences.peakMs);
    Serial.print(" inspect.release_ms=");
    Serial.print(report.occurrences.releaseMs);
    Serial.print(" inspect.duration_ms=");
    Serial.print(report.occurrences.primaryDurationMs);
    Serial.print(" inspect.strength=");
    Serial.print(report.occurrences.primaryStrength, 1);
    Serial.print(" inspect.confidence=");
    Serial.print(report.occurrences.confidence, 2);
    Serial.print(" inspect.reject_reason=");
    Serial.print(report.occurrences.rejectReason != nullptr ? report.occurrences.rejectReason : "none");
    Serial.print(" inspect.primary_evidence=");
    Serial.print(report.inspection.primaryEvidence != nullptr ? report.inspection.primaryEvidence : "none");
    Serial.print(" inspect.target=");
    Serial.print(report.inspection.moduleTarget != nullptr ? report.inspection.moduleTarget : "unknown");
    Serial.print(" inspect.strength_class=");
    Serial.print(report.inspection.moduleStrengthClass != nullptr ? report.inspection.moduleStrengthClass : "unknown");
    Serial.print(" inspect.main_reject_reason=");
    Serial.println(report.inspection.mainRejectReason != nullptr ? report.inspection.mainRejectReason : "none");
}

void AnalyzerApp::printSequenceSourceCanonical(const AnalyzerReport& report) const {
    if (!shouldPrintSequenceSource(report)) {
        return;
    }

    Serial.print("SEQ_SOURCE");
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
    Serial.println();

    printSequenceSourceSpecCanonical(report);
}

void AnalyzerApp::printSequenceSourceSpecCanonical(const AnalyzerReport& report) const {
    printCanonicalDetectorReportGenericLine("SEQ_SOURCE_SPEC", report);
    if (report.detectorReport != nullptr && report.detectorReport->detectorId != detection::DetectorId::Unknown) {
        printCanonicalDetectorDetailLine("SEQ_SOURCE_SPEC", report);
    }
}

void AnalyzerApp::printSequenceExplainCanonical(const AnalyzerReport& report) const {
    if (!shouldPrintSequenceExplain(report)) {
        return;
    }

    Serial.print("SEQ_EXPLAIN");
    Serial.print(" trial=");
    Serial.print(report.context.trial);
    Serial.print(" profile=");
    Serial.println(report.context.profile != nullptr ? report.context.profile : "unknown");
    printCanonicalDetectorReportGenericLine("SEQ_EXPLAIN", report);
    if (report.detectorReport != nullptr && report.detectorReport->detectorId != detection::DetectorId::Unknown) {
        printCanonicalDetectorDetailLine("SEQ_EXPLAIN", report);
    }
    printCanonicalStageLine("SEQ_EXPLAIN", report, true);
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
    Serial.print(cleanDetectorIdName(summary.detectorId));
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
        _sequenceTest.outputConfig.mode == SeqOutputMode::Explain) {
        printAudioSourceSummary();
        printOccurrenceSummary();
    }
    if (_sequenceTest.outputConfig.verbosity > 0U ||
        _sequenceTest.outputConfig.mode == SeqOutputMode::System ||
        _sequenceTest.outputConfig.mode == SeqOutputMode::Explain) {
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
