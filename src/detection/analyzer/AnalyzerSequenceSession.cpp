#include "../../modes/analyzer/AnalyzerModeApp.h"

#include <Arduino.h>
#include <new>
#include <stdlib.h>

#include "../../app/RuntimeDefaults.h"
#include "../../app/TimingUtils.h"
#include "AnalyzerPassRules.h"

bool waitForEmitterAck(const char* expectedPrefix, unsigned long timeoutMs);

namespace {

constexpr unsigned long kSequenceWarmupMs = 500;
constexpr long kLateOnsetMinMs = 200L;

unsigned long countSelectedSampleDumpTrials(unsigned long totalTrials, unsigned long firstTrials, unsigned long everyNth) {
    if (totalTrials == 0) {
        return 0;
    }

    unsigned long selected = 0;
    for (unsigned long trial = 1; trial <= totalTrials; ++trial) {
        const bool firstSelected = firstTrials > 0 && trial <= firstTrials;
        const bool everySelected = everyNth > 0 && trial % everyNth == 0;
        if (firstSelected || everySelected) {
            ++selected;
        }
    }

    return selected;
}

detection::DetectorId cleanSummaryDetectorId(const AnalyzerReport& report) {
    return report.detectorReport != nullptr ? report.detectorReport->detectorId : detection::DetectorId::Unknown;
}

bool detectorReportMatchesOccurrence(const detection::DetectorReport* detectorReport,
                                    const detection::InspectedOccurrence* inspectedOccurrence) {
    if (detectorReport == nullptr || inspectedOccurrence == nullptr || !inspectedOccurrence->occurrence.present) {
        return false;
    }

    return detection::analyzer::sourceReportMatchesIdentity(
               detectorReport->accepted.occurrenceId,
               inspectedOccurrence->occurrence.occurrenceId,
               detectorReport->accepted.startMs,
               inspectedOccurrence->occurrence.startMs,
               detectorReport->accepted.endMs,
               inspectedOccurrence->occurrence.endMs) ||
           detection::analyzer::sourceReportMatchesIdentity(
               detectorReport->selectedReject.occurrenceId,
               inspectedOccurrence->occurrence.occurrenceId,
               detectorReport->selectedReject.startMs,
               inspectedOccurrence->occurrence.startMs,
               detectorReport->selectedReject.endMs,
               inspectedOccurrence->occurrence.endMs);
}

} // namespace

void AnalyzerApp::startSequenceTest(const PendingSequenceStart& pending) {
    unsigned long totalTrials = pending.totalTrials;
    unsigned long periodMs = pending.periodMs;
    unsigned long windowEndOffsetMs = pending.windowEndOffsetMs;
    unsigned long toneHz = pending.toneHz;
    unsigned long durationMs = pending.durationMs;
    bool quiet = pending.quiet;
    bool showDetails = pending.showDetails;
    SequenceDiagMode diagMode = pending.diagMode;
    const char* setupLabel = pending.setupLabel;
    unsigned long reportSettleMs = pending.reportSettleMs;
    bool sampleDumpEnabled = pending.sampleDumpEnabled;
    unsigned long sampleDumpFirstTrials = pending.sampleDumpFirstTrials;
    unsigned long sampleDumpEveryNth = pending.sampleDumpEveryNth;
    unsigned long sampleDumpLeadMs = pending.sampleDumpLeadMs;
    unsigned long sampleDumpTailMs = pending.sampleDumpTailMs;
    unsigned long sampleDumpStepMs = pending.sampleDumpStepMs;
    unsigned long sampleDumpMaxRows = pending.sampleDumpMaxRows;
    unsigned long startupDelayMs = pending.startupDelayMs;
    detection::DetectionProfileKind profileKind = pending.profileKind;
    bool externalEmitter = pending.externalEmitter;

    if (totalTrials == 0) {
        totalTrials = 1;
    }
    if (periodMs == 0) {
        periodMs = 1;
    }
    if (windowEndOffsetMs < 250) {
        windowEndOffsetMs = 250;
    }
    if (windowEndOffsetMs >= periodMs) {
        windowEndOffsetMs = periodMs > 250 ? periodMs - 250 : periodMs;
    }
    if (externalEmitter && windowEndOffsetMs < durationMs + 500) {
        windowEndOffsetMs = durationMs + 500;
    }
    if (reportSettleMs == 0) {
        reportSettleMs = 1;
    }
    if (sampleDumpStepMs == 0) {
        sampleDumpStepMs = 1;
    }
    if (sampleDumpTailMs < sampleDumpLeadMs) {
        sampleDumpTailMs = sampleDumpLeadMs;
    }
    _sequenceTest.active = true;
    _sequenceTest.quiet = quiet || _sequenceTest.outputConfig.mode == AnalyzerApp::SeqOutputMode::Quiet;
    _sequenceTest.showDetails = showDetails;
    _sequenceTest.externalEmitter = externalEmitter;
    _sequenceTest.profileKind = profileKind;
    const detection::DetectionProfile selectedProfile = effectiveSequenceProfile();
    _sequenceTest.outputConfig = _seqOutputConfig;
    _sequenceTest.diagMode = sequenceDiagModeFromOutputWhen(_sequenceTest.outputConfig.when);
    _sequenceTest.progressLineStarted = false;
    _sequenceTest.totalTrials = totalTrials;
    _sequenceTest.periodMs = periodMs;
    _sequenceTest.windowStartOffsetMs = 0;
    _sequenceTest.windowEndOffsetMs = windowEndOffsetMs;
    _sequenceTest.toneHz = toneHz;
    _sequenceTest.durationMs = durationMs;
    _sequenceTest.startupDelayMs = startupDelayMs;
    _sequenceTest.reportSettleMs = reportSettleMs;
    _sequenceTest.sampleDumpEnabled = sampleDumpEnabled;
    _sequenceTest.sampleDumpFirstTrials = sampleDumpFirstTrials;
    _sequenceTest.sampleDumpEveryNth = sampleDumpEveryNth;
    _sequenceTest.sampleDumpLeadMs = sampleDumpLeadMs;
    _sequenceTest.sampleDumpTailMs = sampleDumpTailMs;
    _sequenceTest.sampleDumpStepMs = sampleDumpStepMs;
    _sequenceTest.sampleDumpMaxRows = sampleDumpMaxRows == 0 ? 1 : sampleDumpMaxRows;
    if (_sequenceTest.sampleDumpMaxRows > SequenceTest::kMaxSampleRows) {
        _sequenceTest.sampleDumpMaxRows = SequenceTest::kMaxSampleRows;
    }
    _sequenceTest.sampleDumpWarned = false;
    clearSequenceSampleDump();

    _detection.resetState();
    _detection.setFrequencyMatchConfig(selectedProfile.frequencyMatch);
    _detection.setScalarTransientConfig(selectedProfile.scalarTransient);
    _detection.setDetectorSelection(selectedProfile.detectorSelection);
    _detection.setInspectionPlan(selectedProfile.inspectionPlan);
    _detection.setInspectionPlan(selectedProfile.inspectionPlan);
    _detection.setFieldStateConfig(selectedProfile.fieldStateConfig);
    _detection.setProfileName(detection::detectionProfileName(selectedProfile.kind));
    _detection.setDiagnosticsEnabled(_sequenceTest.outputConfig.diagnosticsEnabled);
    _sequenceTest.sampleDumpDetectorSelection = selectedProfile.detectorSelection;
    _sequenceTest.sampleDumpObservedStream = selectedProfile.scalarTransient.observedStream;
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(toneHz);
    _freqBandStream.setFrequencyUpdateEverySamples(_sequenceTest.outputConfig.frequencyUpdateEverySamples);
    _freqBandStream.resetState();
    _sequenceTest.outputConfig = _seqOutputConfig;

    if (setupLabel != nullptr && setupLabel[0] != '\0') {
        strncpy(_sequenceTest.setupLabel, setupLabel, sizeof(_sequenceTest.setupLabel));
        _sequenceTest.setupLabel[sizeof(_sequenceTest.setupLabel) - 1] = '\0';
    } else {
        strncpy(_sequenceTest.setupLabel, TEST_SETUP_LABEL, sizeof(_sequenceTest.setupLabel));
        _sequenceTest.setupLabel[sizeof(_sequenceTest.setupLabel) - 1] = '\0';
    }

    if (_sequenceTest.sampleDumpEnabled) {
        const unsigned long selectedTrialsEstimate = countSelectedSampleDumpTrials(totalTrials, sampleDumpFirstTrials, sampleDumpEveryNth);
        const unsigned long rowsPerTrial = ((sampleDumpLeadMs + sampleDumpTailMs) / sampleDumpStepMs) + 1UL;
        const unsigned long requestedRows = selectedTrialsEstimate * rowsPerTrial;
        const unsigned long maxAllowedRows = _sequenceTest.sampleDumpMaxRows < SequenceTest::kMaxSampleRows
            ? _sequenceTest.sampleDumpMaxRows
            : SequenceTest::kMaxSampleRows;
        if (requestedRows > _sequenceTest.sampleDumpMaxRows || rowsPerTrial > SequenceTest::kMaxSampleRows) {
            Serial.print("SAMPLES_WARN reason=too_many_samples requested=");
            Serial.print(requestedRows);
            Serial.print(" max_allowed=");
            Serial.println(maxAllowedRows);
            _sequenceTest.sampleDumpEnabled = false;
        }
    }
    _sequenceTest.startedAtMs = millis();
    _sequenceTest.nextTriggerAtMs = _sequenceTest.startedAtMs + _sequenceTest.startupDelayMs;
    _sequenceTest.currentTrial = 0;
    _sequenceTest.currentTrialScheduledAtMs = 0;
    _sequenceTest.currentTrialStartMs = 0;
    _sequenceTest.currentTrialEndMs = 0;
    _sequenceTest.currentTrialOnsetDetectedMs = 0;
    _sequenceTest.currentTrialPatternDetectedMs = 0;
    _sequenceTest.primaryValidPatternCaptured = false;
    _sequenceTest.primaryValidPattern = {};
    _sequenceTest.primaryValidInspectedOccurrence = {};
    _sequenceTest.primaryValidDetectorReport = {};
    _sequenceTest.primaryValidPatternDtMs = -1;
    _sequenceTest.primaryAcceptedOccurrenceCaptured = false;
    _sequenceTest.primaryAcceptedInspectedOccurrence = {};
    _sequenceTest.primaryAcceptedDetectorReport = {};
    _sequenceTest.primaryAcceptedOccurrenceDtMs = -1;
    _sequenceTest.rejectedInWindowCount = 0;
    _sequenceTest.bestRejectedPatternCaptured = false;
    _sequenceTest.bestRejectedInWindow = {};
    _sequenceTest.bestRejectedInspectedOccurrence = {};
    _sequenceTest.bestRejectedDetectorReport = {};
    _sequenceTest.currentTrialFinalized = false;
    _sequenceTest.currentTrialUnexpected = 0;
    _sequenceTest.currentTrialRejected = 0;
    _sequenceTest.currentTrialDiagnostics = {};
    _sequenceTest.hits = 0;
    _sequenceTest.expectedHits = 0;
    _sequenceTest.lateHits = 0;
    _sequenceTest.misses = 0;
    _sequenceTest.unexpected = 0;
    _sequenceTest.duplicates = 0;
    _sequenceTest.startupArtifacts = 0;
    _sequenceTest.samplesProcessed = 0;
    _sequenceTest.currentTrialSamplesProcessed = 0;
    _sequenceTest.maxSamplesPerLoop = 0;
    _sequenceTest.emptySourceLoops = 0;
    _sequenceTest.availableBytesSum = 0;
    _sequenceTest.availableBytesSamples = 0;
    _sequenceTest.maxAvailableBytes = 0;
    _sequenceTest.maxBlockAgeMs = 0;
    _sequenceTest.maxUpdateLoopUs = 0;
    _sequenceTest.totalUpdateLoopUs = 0;
    _sequenceTest.updateLoopCount = 0;
    _sequenceTest.currentTrialUpdateLoopMaxUs = 0;
    _sequenceTest.maxSampleWorkUs = 0;
    _sequenceTest.maxFinalizeTrialUs = 0;
    _sequenceTest.maxProcessingLagMs = 0;
    _sequenceTest.completedTrials = 0;
    _sequenceTest.cleanSummary = {};

    if (!_sequenceTest.externalEmitter) {
        // Rebase before the first trial so every run starts from the quiet floor.
        const unsigned long sequenceClaimSendMs = millis();
        sendEmitterCommand("MODE REMOTE");
        const bool sequenceClaimAcked = waitForEmitterAck("OK MODE REMOTE", 1500);
        const unsigned long sequenceClaimAckMs = millis();
        if (_sequenceTest.showDetails && !_sequenceTest.quiet) {
            Serial.print("SEQ remote claim: send=");
            Serial.print(sequenceClaimSendMs);
            Serial.print("ms ack=");
            Serial.print(sequenceClaimAckMs);
            Serial.print("ms wait=");
            Serial.print(sequenceClaimAckMs - sequenceClaimSendMs);
            Serial.print("ms status=");
            Serial.println(sequenceClaimAcked ? "ok" : "timeout");
        }
    }

    const unsigned long sequenceRebaseStartMs = millis();
    delay(100);
    _audioSignal.rebase();
    if (_sequenceTest.showDetails && !_sequenceTest.quiet) {
        Serial.print("SEQ rebase: start=");
        Serial.print(sequenceRebaseStartMs);
        Serial.print("ms end=");
        Serial.print(millis());
        Serial.print("ms elapsed=");
        Serial.print(millis() - sequenceRebaseStartMs);
        Serial.println("ms");
    }
    resetAudioSignalState();
    _detection.setDiagnosticsEnabled(_sequenceTest.outputConfig.diagnosticsEnabled);
    _audioSignal.resetStats();
    _audioSource.resetStats();
    if (!_sequenceTest.quiet) {
        Serial.println("AUDIO stats reset");
    }

    if (!_sequenceTest.quiet) {
        Serial.print("SEQ start test=");
        Serial.print(_sequenceTest.setupLabel);
        Serial.print(" startup_delay_ms=");
        Serial.print(_sequenceTest.startupDelayMs);
        Serial.print(" report_settle_ms=");
        Serial.print(_sequenceTest.reportSettleMs);
        Serial.print(" loopDelayMs=");
        Serial.print(TEST_LOOP_DELAY_MS);
        Serial.print(" logStress=");
        Serial.println(TEST_LOG_STRESS ? 1 : 0);
    }

    if (_sequenceTest.showDetails && !_sequenceTest.quiet) {
        Serial.print("SEQ start source=");
        Serial.print("I2S");
        Serial.print(" probe=AMP");
        Serial.print(" profile=");
        Serial.print(detection::detectionProfileName(_sequenceTest.profileKind));
        Serial.print(" source=");
        Serial.print(detection::detectorSelectionName(selectedProfile.detectorSelection));
        Serial.print(" detector=");
        Serial.print(detection::detectorSelectionName(selectedProfile.detectorSelection));
        Serial.print(" freq_min_duration_ms=");
        Serial.print(selectedProfile.frequencyMatch.minDurationMs);
        Serial.print(" freq_release_debounce_ms=");
        Serial.print(selectedProfile.frequencyMatch.releaseDebounceMs);
        Serial.print(" freq_cooldown_ms=");
        Serial.print(selectedProfile.frequencyMatch.cooldownAfterReleaseMs);
        //PARAM TUNING TEMPORARY
        Serial.print(" scalar_observed_stream=");
        Serial.print(scalarObservedStreamDisplayName(selectedProfile.scalarTransient.observedStream));
        Serial.print(" scalar_onset_threshold=");
        Serial.print(selectedProfile.scalarTransient.onsetDetectionThreshold, 1);
        Serial.print(" scalar_release_threshold=");
        Serial.print(selectedProfile.scalarTransient.onsetReleaseThreshold, 1);
        Serial.print(" scalar_cooldown_ms=");
        Serial.print(selectedProfile.scalarTransient.cooldownAfterOnsetMs);
        Serial.print(" scalar_min_duration_ms=");
        Serial.print(selectedProfile.scalarTransient.minTransientDurationMs);
        Serial.print(" scalar_max_duration_ms=");
        Serial.print(selectedProfile.scalarTransient.maxTransientDurationMs);
        Serial.print(" scalar_min_peak_strength=");
        Serial.print(selectedProfile.scalarTransient.minTransientPeakStrength, 1);
        Serial.print(" scalar_release_debounce_ms=");
        Serial.print(selectedProfile.scalarTransient.releaseDebounceMs);
        Serial.print(" scalar_require_carrier_quality=");
        Serial.print(selectedProfile.scalarTransient.requireCarrierQuality ? 1 : 0);
        Serial.print(" scalar_min_release_coverage_ms=");
        Serial.print(selectedProfile.scalarTransient.minCoverageAboveReleaseMs);
        Serial.print(" scalar_min_longest_island_ms=");
        Serial.print(selectedProfile.scalarTransient.minLongestIslandMs);
        Serial.print(" scalar_max_gap_ms=");
        Serial.print(selectedProfile.scalarTransient.maxGapMs);
        Serial.print(" history.bin_ms=");
        Serial.print(1);
        Serial.print(" history.capacity_bins=");
        Serial.print(detection::FeatureHistory::kBinsPerStream);
        Serial.print(" history.coverage_ms=");
        Serial.print(detection::FeatureHistory::kBinsPerStream);
        Serial.print(" history.stream=amp aggregation=mean_abs+rms+peak_abs");
        Serial.print(" history.stream=freq_target aggregation=fresh_only");
        Serial.print(" history.stream=freq_contrast aggregation=fresh_only");
        Serial.print(" mode=");
        Serial.print(_sequenceTest.externalEmitter ? "OBS" : "SEQ");
        Serial.print(" test=");
        Serial.print(_sequenceTest.setupLabel);
        Serial.print(" startup_delay_ms=");
        Serial.print(_sequenceTest.startupDelayMs);
        Serial.print(" report_settle_ms=");
        Serial.print(_sequenceTest.reportSettleMs);
        Serial.print(" loopDelayMs=");
        Serial.print(TEST_LOOP_DELAY_MS);
        Serial.print(" logStress=");
        Serial.print(TEST_LOG_STRESS ? 1 : 0);
        Serial.print(" quiet=");
        Serial.print(_sequenceTest.quiet ? 1 : 0);
        Serial.print(" tries=");
        Serial.print(totalTrials);
        Serial.print(" period_ms=");
        Serial.print(periodMs);
        Serial.print(" window_start_ms=");
        Serial.print(_sequenceTest.windowStartOffsetMs);
        Serial.print(" window_end_ms=");
        Serial.print(windowEndOffsetMs);
        Serial.print(" freq_hz=");
        Serial.print(toneHz);
        Serial.print(" dur_ms=");
        Serial.println(durationMs);
        printDetectionParameters();
        if (!_sequenceTest.quiet) {
            Serial.println(_sequenceTest.externalEmitter ? "OBS running" : "SEQ running");
        }
    }
}

void AnalyzerApp::stopSequenceTest() {
    _sequenceTest.active = false;
    _sequenceTest.sampleDumpCapturing = false;
}

void AnalyzerApp::updateSequenceTest(unsigned long now) {
    if (!_sequenceTest.active) {
        return;
    }

    if (_sequenceTest.currentTrial > 0 && !_sequenceTest.currentTrialFinalized) {
        const unsigned long finalizeAtMs = _sequenceTest.currentTrialEndMs + _sequenceTest.reportSettleMs;
        if (timing::beforeDeadline(now, finalizeAtMs)) {
            return;
        }
        finalizeSequenceTrial(now);
    }

    if (!_sequenceTest.active) {
        return;
    }

    if (timing::beforeDeadline(now, _sequenceTest.nextTriggerAtMs)) {
        return;
    }

    if (_sequenceTest.currentTrial >= _sequenceTest.totalTrials) {
        return;
    }

    const unsigned long trialNumber = _sequenceTest.currentTrial + 1;
    const unsigned long scheduledAtMs = _sequenceTest.nextTriggerAtMs;
    _sequenceTest.currentTrial = trialNumber;
    _sequenceTest.currentTrialScheduledAtMs = scheduledAtMs;
    _sequenceTest.currentTrialStartMs = scheduledAtMs;
    _sequenceTest.currentTrialEndMs = scheduledAtMs + _sequenceTest.windowEndOffsetMs;
    _sequenceTest.currentTrialOnsetDetectedMs = 0;
    _sequenceTest.currentTrialPatternDetectedMs = 0;
    _sequenceTest.primaryValidPatternCaptured = false;
    _sequenceTest.primaryValidPattern = {};
    _sequenceTest.primaryValidInspectedOccurrence = {};
    _sequenceTest.primaryValidDetectorReport = {};
    _sequenceTest.primaryValidPatternDtMs = -1;
    _sequenceTest.primaryAcceptedOccurrenceCaptured = false;
    _sequenceTest.primaryAcceptedInspectedOccurrence = {};
    _sequenceTest.primaryAcceptedDetectorReport = {};
    _sequenceTest.primaryAcceptedOccurrenceDtMs = -1;
    _sequenceTest.rejectedInWindowCount = 0;
    _sequenceTest.bestRejectedPatternCaptured = false;
    _sequenceTest.bestRejectedInWindow = {};
    _sequenceTest.bestRejectedInspectedOccurrence = {};
    _sequenceTest.bestRejectedDetectorReport = {};
    _sequenceTest.currentTrialFinalized = false;
    _sequenceTest.currentTrialUnexpected = 0;
    _sequenceTest.currentTrialRejected = 0;
    _sequenceTest.bufferOverrun = false;
    _sequenceTest.trialOverflowCountAtStart = _audioSource.stats().overflowCount;
    _sequenceTest.currentTrialDiagnostics = {};
    _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = _audioSignal.baseline();
    _sequenceTest.currentTrialDiagnostics.runtimePatternCaptured = false;
    _sequenceTest.currentTrialSamplesProcessed = 0;
    _sequenceTest.currentTrialUpdateLoopMaxUs = 0;
    _sequenceTest.totalUpdateLoopUs = 0;
    _sequenceTest.updateLoopCount = 0;
    _detection.resetSourceRejectSummaries();
    resetLoopHealthWindow();
    printSequenceTrialHeader(trialNumber);
    if (_sequenceTest.outputConfig.diagnosticsEnabled) {
        _detection.resetDiagnosticsCounters();
    }
    _sequenceTest.nextTriggerAtMs = scheduledAtMs + _sequenceTest.periodMs;

    beginSequenceSampleDump(trialNumber);

    if (!_sequenceTest.externalEmitter) {
        char command[64];
        snprintf(command, sizeof(command), "CHIRP trial=%lu freq=%lu dur=%lu", trialNumber, _sequenceTest.toneHz, _sequenceTest.durationMs);
        sendEmitterCommand(command);
    }
}

void AnalyzerApp::updateSequenceAmbientStats(unsigned long nowMs) {
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0 || _sequenceTest.currentTrialFinalized) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    const float baseline = _audioSignal.baseline();
    const int signalLevel = _audioSignal.smoothedSignalMagnitude();

    if (diagnostics.ambientBaselineSamples == 0) {
        diagnostics.ambientBaselineMin = baseline;
        diagnostics.ambientBaselineMax = baseline;
        diagnostics.maxSignalLevel = signalLevel;
        diagnostics.ampPeakMs = nowMs;
    } else {
        if (baseline < diagnostics.ambientBaselineMin) {
            diagnostics.ambientBaselineMin = baseline;
        }
        if (baseline > diagnostics.ambientBaselineMax) {
            diagnostics.ambientBaselineMax = baseline;
        }
        if (signalLevel > diagnostics.maxSignalLevel) {
            diagnostics.maxSignalLevel = signalLevel;
            diagnostics.ampPeakMs = nowMs;
        }
    }

    diagnostics.ambientBaselineSamples++;
    diagnostics.ambientBaselineSum += static_cast<float>(signalLevel);
}

AnalyzerApp::SequenceTrialSelection AnalyzerApp::selectSequenceTrialSelection(unsigned long trialOnsetAnchorMs) const {
    SequenceTrialSelection selection = {};

    if (_sequenceTest.primaryValidPatternCaptured) {
        selection.kind = SequenceTrialSelection::Kind::ValidPattern;
        selection.patternResult = &_sequenceTest.primaryValidPattern;
        selection.inspectedOccurrence = _sequenceTest.primaryValidInspectedOccurrence.occurrence.present
            ? &_sequenceTest.primaryValidInspectedOccurrence
            : nullptr;
        selection.detectorReport = _sequenceTest.primaryValidDetectorReport.detectorId != detection::DetectorId::Unknown
            ? &_sequenceTest.primaryValidDetectorReport
            : nullptr;
        selection.occurrenceId = selection.inspectedOccurrence != nullptr
            ? selection.inspectedOccurrence->occurrence.occurrenceId
            : 0UL;
        selection.reportMatched = detectorReportMatchesOccurrence(selection.detectorReport, selection.inspectedOccurrence);
        selection.dtMs = static_cast<long>(_sequenceTest.primaryValidPattern.primaryStartMs) - static_cast<long>(trialOnsetAnchorMs);
        selection.durationMs = _sequenceTest.primaryValidPattern.primaryDurationMs;
        selection.strength = _sequenceTest.primaryValidPattern.primaryStrength;
        selection.result = selection.dtMs >= kLateOnsetMinMs ? AnalyzerResult::Late : AnalyzerResult::Expected;
        return selection;
    }

    if (_sequenceTest.bestRejectedPatternCaptured) {
        selection.kind = SequenceTrialSelection::Kind::RejectedPattern;
        selection.patternResult = &_sequenceTest.bestRejectedInWindow;
        selection.inspectedOccurrence = _sequenceTest.bestRejectedInspectedOccurrence.occurrence.present
            ? &_sequenceTest.bestRejectedInspectedOccurrence
            : nullptr;
        selection.detectorReport = _sequenceTest.bestRejectedDetectorReport.detectorId != detection::DetectorId::Unknown
            ? &_sequenceTest.bestRejectedDetectorReport
            : nullptr;
        selection.occurrenceId = selection.inspectedOccurrence != nullptr
            ? selection.inspectedOccurrence->occurrence.occurrenceId
            : 0UL;
        selection.reportMatched = detectorReportMatchesOccurrence(selection.detectorReport, selection.inspectedOccurrence);
        selection.dtMs = static_cast<long>(_sequenceTest.bestRejectedInWindow.primaryStartMs) - static_cast<long>(trialOnsetAnchorMs);
        selection.durationMs = _sequenceTest.bestRejectedInWindow.primaryDurationMs;
        selection.strength = _sequenceTest.bestRejectedInWindow.primaryStrength;
        selection.result = AnalyzerResult::Rejected;
        return selection;
    }

    if (_sequenceTest.primaryAcceptedOccurrenceCaptured && _sequenceTest.primaryAcceptedInspectedOccurrence.occurrence.present) {
        selection.kind = SequenceTrialSelection::Kind::AcceptedOccurrence;
        selection.inspectedOccurrence = &_sequenceTest.primaryAcceptedInspectedOccurrence;
        selection.detectorReport = _sequenceTest.primaryAcceptedDetectorReport.detectorId != detection::DetectorId::Unknown
            ? &_sequenceTest.primaryAcceptedDetectorReport
            : nullptr;
        selection.occurrenceId = selection.inspectedOccurrence->occurrence.occurrenceId;
        selection.reportMatched = detectorReportMatchesOccurrence(selection.detectorReport, selection.inspectedOccurrence);
        selection.dtMs = static_cast<long>(_sequenceTest.primaryAcceptedInspectedOccurrence.occurrence.startMs) - static_cast<long>(trialOnsetAnchorMs);
        selection.durationMs = _sequenceTest.primaryAcceptedInspectedOccurrence.occurrence.durationMs;
        selection.strength = _sequenceTest.primaryAcceptedInspectedOccurrence.occurrence.strength;
        selection.result = selection.dtMs >= kLateOnsetMinMs ? AnalyzerResult::Late : AnalyzerResult::Expected;
        return selection;
    }

    if (_sequenceTest.currentTrialUnexpected > 0) {
        selection.kind = SequenceTrialSelection::Kind::Unexpected;
        selection.result = AnalyzerResult::Unexpected;
        return selection;
    }

    selection.kind = SequenceTrialSelection::Kind::Miss;
    selection.result = AnalyzerResult::Miss;
    return selection;
}

void AnalyzerApp::finalizeSequenceTrial(unsigned long now) {
    if (_sequenceTest.currentTrial == 0) {
        return;
    }

    if (_sequenceTest.currentTrialFinalized) {
        return;
    }

    // Flush late emitter markers before snapshotting the trial.
    pollEmitterSerial();
    const uint32_t finalizeStartUs = micros();
    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;

    const bool bufferOverrunTrial = _sequenceTest.bufferOverrun
                                    || _audioSource.stats().overflowCount != _sequenceTest.trialOverflowCountAtStart;
    const long trialOnsetAnchorMs = static_cast<long>(sequenceTrialOnsetAnchorMs());
    const SequenceTrialSelection selectedTrial = selectSequenceTrialSelection(trialOnsetAnchorMs);
    const bool rejectedTrial = _sequenceTest.rejectedInWindowCount > 0;
    const bool unexpectedTrial = selectedTrial.result == AnalyzerResult::Unexpected;

    AnalyzerResult result = selectedTrial.result;
    if (selectedTrial.patternResult != nullptr && !selectedTrial.patternResult->valid) {
        result = AnalyzerResult::Rejected;
    }
    long dtMs = selectedTrial.dtMs;
    long durMs = static_cast<long>(selectedTrial.durationMs);
    float strength = selectedTrial.strength;

    const bool confirmedTrial = result == AnalyzerResult::Expected || result == AnalyzerResult::Late;
    if (confirmedTrial) {
        _sequenceTest.hits++;
        if (selectedTrial.kind == SequenceTrialSelection::Kind::ValidPattern) {
            _sequenceTest.primaryValidPatternDtMs = dtMs;
        } else if (selectedTrial.kind == SequenceTrialSelection::Kind::AcceptedOccurrence) {
            _sequenceTest.primaryAcceptedOccurrenceDtMs = dtMs;
        }
        if (result == AnalyzerResult::Late) {
            _sequenceTest.lateHits++;
        } else {
            _sequenceTest.expectedHits++;
        }
    } else if (rejectedTrial) {
        result = AnalyzerResult::Rejected;
    } else if (unexpectedTrial) {
        _sequenceTest.unexpected++;
        result = AnalyzerResult::Unexpected;
    } else {
        result = AnalyzerResult::Miss;
    }

    _sequenceTest.duplicates += diagnostics.duplicateCount;
    AnalyzerReport* finalizedReport = sequenceReportScratch();
    buildSequenceAnalyzerReport(*finalizedReport, _sequenceTest.currentTrial, result, dtMs, durMs, strength, bufferOverrunTrial, diagnostics.duplicateCount, diagnostics);
    updateCleanSequenceSummary(*finalizedReport);
    _sequenceTest.completedTrials++;
    if (result == AnalyzerResult::Miss) {
        if (finalizedReport->debug.startupArtifact) {
            _sequenceTest.startupArtifacts++;
        } else {
            _sequenceTest.misses++;
        }
    }
    flushSequenceSampleHistory(now + 1UL);
    if (shouldPrintSequenceTrial()) {
        printSequenceTrial(*finalizedReport);
    }
    if (_sequenceTest.sampleDumpEnabled) {
        printSequenceSampleReport(_sequenceTest.currentTrial);
    }
    if (shouldPrintSequenceInspect(*finalizedReport)) {
        printSequenceInspectCanonical(*finalizedReport);
    }
    if (shouldPrintSequenceSource(*finalizedReport)) {
        printSequenceSourceCanonical(*finalizedReport);
    }
    if (shouldPrintSequenceSystem(*finalizedReport)) {
        printSystemHealth(*finalizedReport);
    }
    if (shouldPrintSequenceDetail(*finalizedReport)) {
        printSequenceDetailCanonical(*finalizedReport);
    }
    if (_sequenceTest.currentTrial < _sequenceTest.totalTrials) {
        const unsigned long settleUntilMs = now + _sequenceTest.reportSettleMs;
        if (_sequenceTest.nextTriggerAtMs < settleUntilMs) {
            _sequenceTest.nextTriggerAtMs = settleUntilMs;
        }
    }
    _sequenceTest.currentTrialFinalized = true;

    if (_sequenceTest.currentTrial >= _sequenceTest.totalTrials) {
        printSequenceSummaryClean();
        stopSequenceTest();
    }

    const unsigned long finalizeUs = static_cast<unsigned long>(micros() - finalizeStartUs);
    if (finalizeUs > _sequenceTest.maxFinalizeTrialUs) {
        _sequenceTest.maxFinalizeTrialUs = finalizeUs;
    }
}

void AnalyzerApp::updateCleanSequenceSummary(const AnalyzerReport& report) {
    AnalyzerCleanSummary& summary = _sequenceTest.cleanSummary;
    summary.profileName = activeAnalyzerProfileName();
    if (summary.detectorId == detection::DetectorId::Unknown) {
        summary.detectorId = cleanSummaryDetectorId(report);
    }
    summary.trials = static_cast<unsigned int>(_sequenceTest.totalTrials);

    switch (report.classification.result) {
        case AnalyzerResult::Expected:
            ++summary.expectedTrials;
            break;
        case AnalyzerResult::Early:
            ++summary.earlyTrials;
            break;
        case AnalyzerResult::Late:
            ++summary.lateTrials;
            break;
        case AnalyzerResult::Miss:
            ++summary.missTrials;
            break;
        case AnalyzerResult::Unexpected:
            ++summary.unexpectedTrials;
            break;
        case AnalyzerResult::Rejected:
            ++summary.rejectedTrials;
            break;
        case AnalyzerResult::Ambiguous:
            ++summary.ambiguousTrials;
            break;
        case AnalyzerResult::TooDense:
            ++summary.tooDenseTrials;
            break;
        case AnalyzerResult::Unknown:
        default:
            break;
    }

    if (report.classification.result == AnalyzerResult::Duplicate) {
        ++summary.duplicateTrials;
    }

    if (report.debug.bufferOverrun) {
        ++summary.bufferOverrunTrials;
    }

    if (report.detectorReport != nullptr) {
        if (report.detectorReport->accepted.present) {
            ++summary.detectorAcceptedTrials;
        }
        if (report.detectorReport->selectedReject.present) {
            ++summary.detectorSelectedRejectTrials;
        }
    }

    if (report.primaryPattern.accepted) {
        ++summary.patternValidTrials;
    } else if (report.primaryPattern.patternAccepted || report.classification.result == AnalyzerResult::Rejected) {
        ++summary.patternRejectedTrials;
    }

    if (report.classification.dtMs >= 0) {
        summary.totalDtMs += report.classification.dtMs;
        ++summary.dtCount;
    }

    if (report.occurrences.primaryStrength > 0.0f) {
        summary.totalStrength += report.occurrences.primaryStrength;
        ++summary.strengthCount;
    }

    if (report.primaryPattern.confidence > 0.0f) {
        summary.totalConfidence += report.primaryPattern.confidence;
        ++summary.confidenceCount;
    }

    summary.completed = detection::analyzer::completedPrimaryTrialCount(summary);
}

unsigned long AnalyzerApp::sequenceTrialOnsetAnchorMs() const {
    if (_sequenceTest.currentTrialDiagnostics.emitStartSeen) {
        return _sequenceTest.currentTrialStartMs + _sequenceTest.currentTrialDiagnostics.emitStartDtMs;
    }
    return _sequenceTest.currentTrialScheduledAtMs;
}


