#include "AnalyzerApp.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "AnalyzerHealthHelpers.h"

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

void AnalyzerApp::printDetectionParameters() const {
    //PARAM TUNING TEMPORARY
    const detection::DetectionProfile activeProfile = effectiveSequenceProfile();

    const detection::ScalarTransientConfig& scalar = activeProfile.scalarTransient;
    Serial.print("SEQ scalar:");
    Serial.print(" observed_stream=");
    Serial.print(scalarObservedStreamDisplayName(scalar.observedStream));
    Serial.print(" onset_threshold=");
    Serial.print(scalar.onsetDetectionThreshold, 1);
    Serial.print(" release_threshold=");
    Serial.print(scalar.onsetReleaseThreshold, 1);
    Serial.print(" cooldown_ms=");
    Serial.print(scalar.cooldownAfterOnsetMs);
    Serial.print(" min_duration_ms=");
    Serial.print(scalar.minTransientDurationMs);
    Serial.print(" max_duration_ms=");
    Serial.print(scalar.maxTransientDurationMs);
    Serial.print(" min_peak_strength=");
    Serial.print(scalar.minTransientPeakStrength, 1);
    Serial.print(" release_debounce_ms=");
    Serial.println(scalar.releaseDebounceMs);

    const unsigned long sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const unsigned long windowSizeSamples = _freqBandStream.windowSizeSamples();
    const unsigned long frequencyUpdateEverySamples = _freqBandStream.frequencyUpdateEverySamples();
    const unsigned long ageSamples = _freqBandStream.lastPacketAgeSamples();
    const float windowMs = sampleRateHz > 0
        ? (static_cast<float>(windowSizeSamples) * 1000.0f) / static_cast<float>(sampleRateHz)
        : 0.0f;
    const float updateStepMs = sampleRateHz > 0
        ? (static_cast<float>(frequencyUpdateEverySamples) * 1000.0f) / static_cast<float>(sampleRateHz)
        : 0.0f;
    const float ageMs = sampleRateHz > 0
        ? (static_cast<float>(ageSamples) * 1000.0f) / static_cast<float>(sampleRateHz)
        : 0.0f;

    Serial.print("FREQBAND runtime:");
    Serial.print(" freq.window_samples=");
    Serial.print(windowSizeSamples);
    Serial.print(" freq.window_ms=");
    Serial.print(windowMs, 2);
    Serial.print(" freq.update_every_samples=");
    Serial.print(frequencyUpdateEverySamples);
    Serial.print(" freq.update_period_ms=");
    Serial.print(updateStepMs, 3);
    Serial.print(" freq.target_hz=");
    Serial.print(_freqBandStream.targetFrequencyHz());
    Serial.print(" freq.produced_fresh_packet=");
    Serial.print(_freqBandStream.producedFreshPacketOnLastObserve() ? 1 : 0);
    Serial.print(" freq.packet_age_samples=");
    Serial.print(ageSamples);
    Serial.print(" freq.packet_age_ms=");
    Serial.println(ageMs, 3);
}

void AnalyzerApp::printParamStatus() const {
    const detection::DetectionProfile activeProfile = effectiveSequenceProfile();

    const detection::ScalarTransientConfig& scalar = activeProfile.scalarTransient;
    Serial.print("PARAM scalar_observed_stream=");
    Serial.print(scalarObservedStreamDisplayName(scalar.observedStream));
    Serial.print(" scalar_onset_threshold=");
    Serial.print(scalar.onsetDetectionThreshold, 1);
    Serial.print(" scalar_release_threshold=");
    Serial.print(scalar.onsetReleaseThreshold, 1);
    Serial.print(" scalar_cooldown_ms=");
    Serial.print(scalar.cooldownAfterOnsetMs);
    Serial.print(" scalar_min_duration_ms=");
    Serial.print(scalar.minTransientDurationMs);
    Serial.print(" scalar_max_duration_ms=");
    Serial.print(scalar.maxTransientDurationMs);
    Serial.print(" scalar_min_peak_strength=");
    Serial.print(scalar.minTransientPeakStrength, 1);
    Serial.print(" scalar_release_debounce_ms=");
    Serial.print(scalar.releaseDebounceMs);

    Serial.print(" freqScore=");
    Serial.print(_analyzerTuning.frequencyMatch.attackScoreMin, 1);
    Serial.print(" freqContrast=");
    Serial.print(_analyzerTuning.frequencyMatch.attackContrastMin, 1);
    Serial.print(" freqReleaseScore=");
    Serial.print(_analyzerTuning.frequencyMatch.releaseScoreMin, 1);
    Serial.print(" freqReleaseContrast=");
    Serial.println(_analyzerTuning.frequencyMatch.releaseContrastMin, 1);
}

void AnalyzerApp::printAudioSourceSummary() const {
    const AudioSourceStats& stats = _audioSource.stats();
    Serial.println("AUDIO summary:");
    Serial.print("reads=");
    Serial.print(stats.reads);
    Serial.print(" readBytes=");
    Serial.print(stats.readBytes);
    Serial.print(" zeroReads=");
    Serial.print(stats.zeroReads);
    Serial.print(" shortReads=");
    Serial.print(stats.shortReads);
    Serial.print(" maxReadBytes=");
    Serial.print(stats.maxReadBytes);
    Serial.print(" noSampleLoops=");
    Serial.print(stats.noSampleLoops);
    Serial.print(" readErrors=");
    Serial.print(stats.readErrors);
    Serial.print(" overflow=");
    Serial.print(stats.overflowCount);
    Serial.print(" droppedBlocks=");
    Serial.print(stats.droppedBlockCount);
    Serial.print(" totalSamples=");
    Serial.println(static_cast<unsigned long long>(stats.totalSamplesRead));
}

void AnalyzerApp::printAudioRunSummary() const {
    if (!_sequenceTest.active && _sequenceTest.completedTrials == 0) {
        return;
    }

    const unsigned long now = millis();
    const unsigned long sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const unsigned long runElapsedMs = _sequenceTest.startedAtMs > 0
        ? (now >= _sequenceTest.startedAtMs ? now - _sequenceTest.startedAtMs : 0UL)
        : 0UL;
    const unsigned long expectedSamples = static_cast<unsigned long>(
        (static_cast<unsigned long long>(runElapsedMs) * static_cast<unsigned long long>(sampleRateHz)) / 1000ULL);
    const unsigned long processedSamples = _sequenceTest.samplesProcessed;
    const float processedRatio = expectedSamples > 0
        ? static_cast<float>(processedSamples) / static_cast<float>(expectedSamples)
        : 0.0f;
    const unsigned long avgAvailableBytes = _sequenceTest.availableBytesSamples > 0
        ? static_cast<unsigned long>(_sequenceTest.availableBytesSum / static_cast<uint64_t>(_sequenceTest.availableBytesSamples))
        : 0UL;

    Serial.println("AUDIO run:");
    Serial.print("run_elapsed_ms=");
    Serial.print(runElapsedMs);
    Serial.print(" expected_samples=");
    Serial.print(expectedSamples);
    Serial.print(" processed_samples=");
    Serial.print(processedSamples);
    Serial.print(" processed_ratio=");
    Serial.print(processedRatio, 3);
    Serial.print(" max_available_bytes=");
    Serial.print(_sequenceTest.maxAvailableBytes);
    Serial.print(" avg_available_bytes=");
    Serial.print(avgAvailableBytes);
    Serial.print(" max_block_age_ms=");
    Serial.print(_sequenceTest.maxBlockAgeMs);
    Serial.print(" max_update_loop_us=");
    Serial.print(_sequenceTest.maxUpdateLoopUs);
    Serial.print(" max_processing_lag_ms=");
    Serial.println(_sequenceTest.maxProcessingLagMs);

    Serial.print("FREQBAND config:");
    Serial.print(" freqband=");
    Serial.print(_seqOutputConfig.frequencyBandEnabled ? "on" : "off");
    Serial.print(" updateEverySamples=");
    Serial.println(_seqOutputConfig.frequencyUpdateEverySamples);

    const unsigned long freqObserveCalls = _freqBandStream.profileObserveCalls();
    const unsigned long freqComputeCalls = _freqBandStream.profileComputeCalls();
    const float avgObserveUs = freqObserveCalls > 0
        ? static_cast<float>(_freqBandStream.profileObserveTotalUs()) / static_cast<float>(freqObserveCalls)
        : 0.0f;
    const float avgComputeUs = freqComputeCalls > 0
        ? static_cast<float>(_freqBandStream.profileComputeTotalUs()) / static_cast<float>(freqComputeCalls)
        : 0.0f;
    const float avgEnergyUs = freqComputeCalls > 0
        ? static_cast<float>(_freqBandStream.profileEnergyTotalUs()) / static_cast<float>(freqComputeCalls)
        : 0.0f;
    const float avgGoertzelUs = freqComputeCalls > 0
        ? static_cast<float>(_freqBandStream.profileGoertzelTotalUs()) / static_cast<float>(freqComputeCalls)
        : 0.0f;

    Serial.print("FREQBAND profile:");
    Serial.print(" observe_calls=");
    Serial.print(freqObserveCalls);
    Serial.print(" avg_observe_us=");
    Serial.print(avgObserveUs, 2);
    Serial.print(" compute_calls=");
    Serial.print(freqComputeCalls);
    Serial.print(" avg_compute_us=");
    Serial.print(avgComputeUs, 2);
    Serial.print(" avg_energy_us=");
    Serial.print(avgEnergyUs, 2);
    Serial.print(" avg_goertzel_us=");
    Serial.println(avgGoertzelUs, 2);

    const unsigned long freqFreshObserveCalls = _freqBandStream.profileComputeCalls();
    const unsigned long freqHeldObserveCalls = _freqBandStream.profileObserveCalls() > _freqBandStream.profileComputeCalls()
        ? _freqBandStream.profileObserveCalls() - _freqBandStream.profileComputeCalls()
        : 0UL;
    const unsigned long freqAgeSamples = _freqBandStream.lastPacketAgeSamples();
    const unsigned long freqComputedAtSample = _freqBandStream.sampleCount() >= freqAgeSamples
        ? _freqBandStream.sampleCount() - freqAgeSamples
        : 0UL;
    const unsigned long freqHistoryScoreRecords =
        _detection.featureHistory().sampleCount(detection::FeatureStreamId::FrequencyScore);
    const unsigned long freqHistoryContrastRecords =
        _detection.featureHistory().sampleCount(detection::FeatureStreamId::FrequencyContrast);

    Serial.print("FREQBAND freshness:");
    Serial.print(" fresh_frames=");
    Serial.print(freqFreshObserveCalls);
    Serial.print(" held_frames=");
    Serial.print(freqHeldObserveCalls);
    Serial.print(" age_samples=");
    Serial.print(freqAgeSamples);
    Serial.print(" computed_at_sample=");
    Serial.print(freqComputedAtSample);
    Serial.print(" history_score_records=");
    Serial.print(freqHistoryScoreRecords);
    Serial.print(" history_contrast_records=");
    Serial.println(freqHistoryContrastRecords);
}

void AnalyzerApp::printOccurrenceSummary() const {
    const AudioSignalStats& stats = _audioSignal.stats();
    Serial.println("OCCURRENCE summary:");
    Serial.print("blocks=");
    Serial.print(stats.blocksProcessed);
    Serial.print(" samples=");
    Serial.print(static_cast<unsigned long long>(stats.samplesProcessed));
    Serial.print(" lastBlockStart=");
    Serial.print(static_cast<unsigned long long>(_audioSignal.lastBlockStartSample()));
    Serial.print(" lastBlockCount=");
    Serial.print(_audioSignal.lastBlockSampleCount());
    Serial.print(" lastBlockMicros=");
    Serial.println(_audioSignal.lastBlockApproxStartMicros());
}

void AnalyzerApp::printSystemHealth(const AnalyzerReport& report) const {
    const AudioSourceStats& sourceStats = _audioSource.stats();
    const AudioSignalStats& signalStats = _audioSignal.stats();
    const unsigned long sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const bool compactHealth = _sequenceTest.outputConfig.verbosity == 0U;
    const bool hasTrialWindow = _sequenceTest.active
        && _sequenceTest.currentTrial > 0
        && _sequenceTest.currentTrialEndMs >= _sequenceTest.currentTrialStartMs;
    const unsigned long windowStartMs = hasTrialWindow
        ? _sequenceTest.currentTrialStartMs
        : activeRunStartMs();
    const unsigned long windowEndMs = hasTrialWindow
        ? _sequenceTest.currentTrialEndMs
        : activeRunEndMs();
    const unsigned long windowElapsedMs = windowStartMs > 0 && windowEndMs >= windowStartMs ? windowEndMs - windowStartMs : 0UL;
    const unsigned long expectedSamples = static_cast<unsigned long>(
        (static_cast<unsigned long long>(windowElapsedMs) * static_cast<unsigned long long>(sampleRateHz)) / 1000ULL);
    const unsigned long processedSamples = hasTrialWindow
        ? _sequenceTest.currentTrialSamplesProcessed
        : (_sequenceTest.active ? _sequenceTest.samplesProcessed
            : static_cast<unsigned long>(signalStats.samplesProcessed));
    const float processedRatio = expectedSamples > 0
        ? static_cast<float>(processedSamples) / static_cast<float>(expectedSamples)
        : 0.0f;
    const unsigned long loopAvgUs = _loopHealth.count > 0 ? static_cast<unsigned long>(_loopHealth.sumUs / _loopHealth.count) : 0UL;
    const unsigned long updateLoopAvgUs = _sequenceTest.updateLoopCount > 0
        ? static_cast<unsigned long>(_sequenceTest.totalUpdateLoopUs / _sequenceTest.updateLoopCount)
        : 0UL;

    Serial.print("SYSTEM_HEALTH");
    Serial.print(" loop_avg_us=");
    Serial.print(loopAvgUs);
    Serial.print(" update_loop_avg_us=");
    Serial.print(updateLoopAvgUs);
    if (!compactHealth) {
        Serial.print(" trial=");
        Serial.print(report.context.trial);
        Serial.print(" t_ms=");
        Serial.print(report.context.timestampMs);
        Serial.print(" uptime_ms=");
        Serial.print(millis());
        Serial.print(" boot_count=");
        Serial.print(analyzerBootCount());
        Serial.print(" reset_reason=");
        Serial.print(currentResetReasonName());
        Serial.print(" window_start_ms=");
        Serial.print(windowStartMs);
        Serial.print(" window_end_ms=");
        Serial.print(windowEndMs);
        Serial.print(" window_elapsed_ms=");
        Serial.print(windowElapsedMs);
        Serial.print(" free_heap=");
        Serial.print(esp_get_free_heap_size());
        Serial.print(" loop_stats_window_ms=");
        Serial.print(windowElapsedMs);
        Serial.print(" loop_stats_scope=");
        Serial.print(_sequenceTest.active && _sequenceTest.currentTrialStartMs > 0 ? "trial_window" : "run_window");
        Serial.print(" loop_count=");
        Serial.print(_loopHealth.count);
        Serial.print(" loop_max_window_us=");
        Serial.print(_loopHealth.maxUs);
        Serial.print(" loop_over_5ms=");
        Serial.print(_loopHealth.over5ms);
        Serial.print(" loop_over_20ms=");
        Serial.print(_loopHealth.over20ms);
        Serial.print(" loop_max_since_boot_us=");
        Serial.print(_loopMaxSinceBootUs);
        Serial.print(" update_loop_count=");
        Serial.print(_sequenceTest.updateLoopCount);
        Serial.print(" update_loop_max_us=");
        Serial.print(_sequenceTest.currentTrialUpdateLoopMaxUs);
        Serial.print(" sample_work_max_us=");
        Serial.print(_sequenceTest.maxSampleWorkUs);
        Serial.print(" trial_finalize_max_us=");
        Serial.print(_sequenceTest.maxFinalizeTrialUs);
    }
    Serial.println();
    _loopHealth.reset();

    Serial.print("AUDIO_IO_HEALTH");
    Serial.print(" processed_ratio=");
    Serial.print(processedRatio, 3);
    if (!compactHealth) {
        Serial.print(" t_ms=");
        Serial.print(report.context.timestampMs);
        Serial.print(" i2s_reads=");
        Serial.print(sourceStats.reads);
        Serial.print(" i2s_bytes_read=");
        Serial.print(sourceStats.readBytes);
        Serial.print(" processed_samples=");
        Serial.print(processedSamples);
        Serial.print(" expected_samples=");
        Serial.print(expectedSamples);
        Serial.print(" frames_emitted=");
        Serial.print(signalStats.blocksProcessed);
        Serial.print(" partial_frames=");
        Serial.print(sourceStats.shortReads);
        Serial.print(" duplicate_frames=0");
        Serial.print(" gap_samples=");
        Serial.print(sourceStats.droppedBlockCount);
        Serial.print(" overlap_samples=0");
        Serial.print(" max_block_age_ms=");
        Serial.print(_sequenceTest.active ? _sequenceTest.maxBlockAgeMs : 0UL);
        Serial.print(" max_processing_lag_ms=");
        Serial.println(_sequenceTest.active ? _sequenceTest.maxProcessingLagMs : 0UL);
    } else {
        Serial.println();
    }

    const auto& rawDiagnostics = _sequenceTest.currentTrialDiagnostics;
    const float rawDcMean = rawDiagnostics.rawFrames > 0
        ? static_cast<float>(rawDiagnostics.rawSum) / static_cast<float>(rawDiagnostics.rawFrames)
        : 0.0f;
    const float rawMeanAbs = rawDiagnostics.rawFrames > 0
        ? static_cast<float>(rawDiagnostics.rawAbsSum) / static_cast<float>(rawDiagnostics.rawFrames)
        : 0.0f;
    const int32_t rawMinAbs = rawDiagnostics.rawMin < 0 ? -static_cast<int32_t>(rawDiagnostics.rawMin) : static_cast<int32_t>(rawDiagnostics.rawMin);
    const int32_t rawMaxAbsValue = rawDiagnostics.rawMax < 0 ? -static_cast<int32_t>(rawDiagnostics.rawMax) : static_cast<int32_t>(rawDiagnostics.rawMax);
    const uint16_t rawMaxAbs = static_cast<uint16_t>(rawMinAbs > rawMaxAbsValue ? rawMinAbs : rawMaxAbsValue);
    const float rawZeroRatio = rawDiagnostics.rawFrames > 0
        ? static_cast<float>(_sequenceTest.currentTrialDiagnostics.audioZeroishFrames) / static_cast<float>(rawDiagnostics.rawFrames)
        : 0.0f;
    const char* rawHealth = rawHealthClassNameFromCounters(
        _sequenceTest.currentTrialDiagnostics.audioRmsTooHighFrames,
        rawDiagnostics.rawFrames,
        rawMaxAbs,
        rawDcMean,
        rawMeanAbs,
        rawDiagnostics.rawMin,
        rawDiagnostics.rawMax,
        0.0f,
        0UL,
        0UL,
        _sequenceTest.currentTrialDiagnostics.audioFlatlineFrames,
        _sequenceTest.currentTrialDiagnostics.audioZeroishFrames,
        _sequenceTest.currentTrialDiagnostics.audioLargeJumpFrames,
        _sequenceTest.currentTrialDiagnostics.audioRms,
        _sequenceTest.currentTrialDiagnostics.audioRmsTooLowFrames,
        _sequenceTest.currentTrialDiagnostics.audioRmsTooHighFrames
    );
    if (_sequenceTest.outputConfig.verbosity >= 1U) {
        Serial.print("RAW_AUDIO_HEALTH");
        Serial.print(" verdict=");
        Serial.print(rawHealth != nullptr ? rawHealth : "unknown");
    }
    if (_sequenceTest.outputConfig.verbosity >= 2U) {
        Serial.print(" t_ms=");
        Serial.print(report.context.timestampMs);
        Serial.print(" frames=");
        Serial.print(rawDiagnostics.rawFrames);
        Serial.print(" min_sample=");
        Serial.print(rawDiagnostics.rawFrames > 0 ? rawDiagnostics.rawMin : 0);
        Serial.print(" max_sample=");
        Serial.print(rawDiagnostics.rawFrames > 0 ? rawDiagnostics.rawMax : 0);
        Serial.print(" dc_mean=");
        Serial.print(rawDcMean, 1);
        Serial.print(" mean_abs=");
        Serial.print(rawMeanAbs, 1);
        Serial.print(" max_abs=");
        Serial.print(rawMaxAbs);
        Serial.print(" zero_ratio=");
        Serial.print(rawZeroRatio, 3);
        Serial.print(" clip_count=");
        Serial.print(_sequenceTest.currentTrialDiagnostics.audioRmsTooHighFrames);
        Serial.print(" flatline=");
        Serial.println(rawHealth != nullptr && strcmp(rawHealth, "flatline") == 0 ? 1 : 0);
    } else if (_sequenceTest.outputConfig.verbosity >= 1U) {
        Serial.println();
    }

    if (!shouldPrintHardwareDiagnostics()) {
        return;
    }

    const AudioSlotDiagnostics& slotDiag = _i2sSource.slotDiagnostics();
    Serial.print("I2S_SLOT_DIAG");
    Serial.print(" slot_diag_source=");
    Serial.print(slotDiag.slotDiagSource != nullptr ? slotDiag.slotDiagSource : "unknown");
    Serial.print(" present=");
    Serial.print(slotDiag.present ? 1 : 0);
    Serial.print(" slot0_samples=");
    Serial.print(slotDiag.slotCount[0]);
    Serial.print(" slot1_samples=");
    Serial.print(slotDiag.slotCount[1]);
    Serial.print(" slot0_signed_min=");
    Serial.print(slotDiag.slotCount[0] > 0 ? slotDiag.slotMin[0] : 0);
    Serial.print(" slot0_signed_max=");
    Serial.print(slotDiag.slotCount[0] > 0 ? slotDiag.slotMax[0] : 0);
    Serial.print(" slot0_signed_range=");
    Serial.print(slotDiag.slotCount[0] > 0 ? static_cast<long>(slotDiag.slotMax[0] - slotDiag.slotMin[0]) : 0L);
    Serial.print(" slot0_signed_rms=");
    Serial.print(slotDiag.slotCount[0] > 0 ? sqrt(slotDiag.slotSumSquares[0] / static_cast<double>(slotDiag.slotCount[0])) : 0.0, 1);
    Serial.print(" slot0_repeated_run=");
    Serial.print(slotDiag.slotRepeatedRun[0]);
    Serial.print(" slot1_signed_min=");
    Serial.print(slotDiag.slotCount[1] > 0 ? slotDiag.slotMin[1] : 0);
    Serial.print(" slot1_signed_max=");
    Serial.print(slotDiag.slotCount[1] > 0 ? slotDiag.slotMax[1] : 0);
    Serial.print(" slot1_signed_range=");
    Serial.print(slotDiag.slotCount[1] > 0 ? static_cast<long>(slotDiag.slotMax[1] - slotDiag.slotMin[1]) : 0L);
    Serial.print(" slot1_signed_rms=");
    Serial.print(slotDiag.slotCount[1] > 0 ? sqrt(slotDiag.slotSumSquares[1] / static_cast<double>(slotDiag.slotCount[1])) : 0.0, 1);
    Serial.print(" slot1_repeated_run=");
    Serial.print(slotDiag.slotRepeatedRun[1]);
    Serial.print(" chosen_slot=");
    Serial.print(slotDiag.chosenSlot != nullptr ? slotDiag.chosenSlot : "none");
    Serial.print(" active_slot=");
    Serial.print(slotDiag.activeSlot != nullptr ? slotDiag.activeSlot : "none");
    Serial.print(" slot_selection_reason=");
    Serial.println(slotDiag.slotSelectionReason != nullptr ? slotDiag.slotSelectionReason : "none");
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
