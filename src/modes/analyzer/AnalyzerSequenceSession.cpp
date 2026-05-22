#include "AnalyzerApp.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "../../AudioDebugConfig.h"
#include "../../TimingUtils.h"

bool waitForEmitterAck(const char* expectedPrefix, unsigned long timeoutMs);

namespace {

constexpr unsigned long kSequenceWarmupMs = 500;
constexpr long kLateOnsetMinMs = 200L;

bool analyzerLogEnabled(uint32_t flags, AnalyzerApp::AnalyzerLogFlags flag) {
    return (flags & static_cast<uint32_t>(flag)) != 0;
}

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

} // namespace

void AnalyzerApp::startSequenceTest(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet, bool showDetails, const char* setupLabel, uint32_t logFlags, bool sampleDumpEnabled, unsigned long sampleDumpFirstTrials, unsigned long sampleDumpEveryNth, unsigned long sampleDumpLeadMs, unsigned long sampleDumpTailMs, unsigned long sampleDumpStepMs, unsigned long sampleDumpMaxRows, detection::DetectionProfileKind profileKind, bool externalEmitter) {
    if (_valMode) {
        return;
    }
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
    if (sampleDumpStepMs == 0) {
        sampleDumpStepMs = 1;
    }
    if (sampleDumpTailMs < sampleDumpLeadMs) {
        sampleDumpTailMs = sampleDumpLeadMs;
    }
    free(_sequenceTest.trialReports);
    _sequenceTest.trialReports = nullptr;
    _sequenceTest.trialReportCapacity = 0;
    _sequenceTest.trialReportCount = 0;

    _sequenceTest.active = true;
    _sequenceTest.quiet = quiet;
    _sequenceTest.showDetails = showDetails;
    _sequenceTest.externalEmitter = externalEmitter;
    _sequenceTest.profileKind = profileKind;
    _sequenceTest.progressLineStarted = false;
    _sequenceTest.logFlags = logFlags;
    _sequenceTest.totalTrials = totalTrials;
    _sequenceTest.periodMs = periodMs;
    _sequenceTest.windowStartOffsetMs = 0;
    _sequenceTest.windowEndOffsetMs = windowEndOffsetMs;
    _sequenceTest.toneHz = toneHz;
    _sequenceTest.durationMs = durationMs;
    _sequenceTest.sampleDumpEnabled = sampleDumpEnabled;
    _sequenceTest.sampleDumpFirstTrials = sampleDumpFirstTrials;
    _sequenceTest.sampleDumpEveryNth = sampleDumpEveryNth;
    _sequenceTest.sampleDumpLeadMs = sampleDumpLeadMs;
    _sequenceTest.sampleDumpTailMs = sampleDumpTailMs;
    _sequenceTest.sampleDumpStepMs = sampleDumpStepMs;
    _sequenceTest.sampleDumpMaxRows = sampleDumpMaxRows == 0 ? 1 : sampleDumpMaxRows;
    _sequenceTest.sampleDumpWarned = false;
    clearSequenceSampleDump();

    if (_detection == nullptr) {
        _detection = new detection::DetectionRuntime();
    }
    _detection->reset();
    _detection->setFrequencyTuning(_frequencyEvidenceTuning);
    const detection::DetectionProfile& selectedProfile = detection::detectionProfileForKind(_sequenceTest.profileKind);
    _detection->setOccurrenceSource(selectedProfile.occurrenceSource);
    _detection->setInspectionRules(selectedProfile.inspectionRules);
    _detection->setInspectionConfig(selectedProfile.inspectionConfig);
    _detection->setPatternRulesConfig(selectedProfile.patternRulesConfig);
    _detection->setFieldStateConfig(selectedProfile.fieldStateConfig);
    _detection->setProfileName(detection::detectionProfileName(selectedProfile.kind));
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(toneHz);
    _freqBandStream.resetState();

    const bool wantSummaryReports =
        analyzerLogEnabled(logFlags, AnalyzerApp::ANALYZER_LOG_SUMMARY);
    if (wantSummaryReports) {
        const size_t desiredCapacity = static_cast<size_t>(totalTrials < SequenceTest::kMaxTrialReports ? totalTrials : SequenceTest::kMaxTrialReports);
        if (desiredCapacity > 0) {
            _sequenceTest.trialReports = static_cast<AnalyzerReport*>(calloc(desiredCapacity, sizeof(AnalyzerReport)));
            if (_sequenceTest.trialReports != nullptr) {
                _sequenceTest.trialReportCapacity = desiredCapacity;
            } else {
                Serial.print("SEQ_VERBOSE_WARN reason=analyzer_report_alloc_failed requested=");
                Serial.print(desiredCapacity);
                Serial.println(" reports");
            }
        }
    }
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
    _sequenceTest.nextTriggerAtMs = _sequenceTest.startedAtMs + kSequenceWarmupMs;
    _sequenceTest.currentTrial = 0;
    _sequenceTest.currentTrialScheduledAtMs = 0;
    _sequenceTest.currentTrialStartMs = 0;
    _sequenceTest.currentTrialEndMs = 0;
    _sequenceTest.currentTrialOnsetDetectedMs = 0;
    _sequenceTest.currentTrialPatternDetectedMs = 0;
    _sequenceTest.primaryValidPatternCaptured = false;
    _sequenceTest.primaryValidPattern = {};
    _sequenceTest.primaryValidPatternDtMs = -1;
    _sequenceTest.rejectedInWindowCount = 0;
    _sequenceTest.firstRejectedInWindow = {};
    _sequenceTest.currentTrialHit = false;
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
    _sequenceTest.invalidAudio = 0;
    _sequenceTest.samplesProcessed = 0;
    _sequenceTest.maxSamplesPerLoop = 0;
    _sequenceTest.emptySourceLoops = 0;
    _sequenceTest.totalHitStrengthScaled = 0;
    _sequenceTest.totalHitDurationMs = 0;
    _sequenceTest.patternMatchedExpected = 0;
    _sequenceTest.patternUnmatchedExpected = 0;
    _sequenceTest.patternMatchedDuplicates = 0;
    _sequenceTest.patternUnmatchedDuplicates = 0;
    _sequenceTest.patternMatchedUnexpected = 0;
    _sequenceTest.patternUnmatchedUnexpected = 0;
    _sequenceTest.freqRejectScore = 0;
    _sequenceTest.freqRejectContrast = 0;
    _sequenceTest.freqRejectBoth = 0;
    _sequenceTest.freqRejectNoEvidence = 0;
    _sequenceTest.freqRejectInvalidWindow = 0;

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
    resetDetectorState();
    _audioSignal.resetStats();
    _audioSource.resetStats();
    Serial.println("AUDIO stats reset");

    Serial.print("SEQ start test=");
    Serial.print(_sequenceTest.setupLabel);
    Serial.print(" warmup_ms=");
    Serial.print(kSequenceWarmupMs);
    Serial.print(" loopDelayMs=");
    Serial.print(TEST_LOOP_DELAY_MS);
    Serial.print(" logStress=");
    Serial.println(TEST_LOG_STRESS ? 1 : 0);

    if (_sequenceTest.showDetails) {
        Serial.print("SEQ start source=");
        Serial.print("I2S");
        Serial.print(" probe=AMP");
        Serial.print(" profile=");
        Serial.print(detection::detectionProfileName(_sequenceTest.profileKind));
        Serial.print(" mode=");
        Serial.print(_sequenceTest.externalEmitter ? "OBS" : "SEQ");
        Serial.print(" test=");
        Serial.print(_sequenceTest.setupLabel);
        Serial.print(" warmup_ms=");
        Serial.print(kSequenceWarmupMs);
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
    free(_sequenceTest.trialReports);
    _sequenceTest.trialReports = nullptr;
    _sequenceTest.trialReportCapacity = 0;
    _sequenceTest.trialReportCount = 0;
}

void AnalyzerApp::updateSequenceTest(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active) {
        return;
    }

    if (_sequenceTest.currentTrial > 0 && timing::atOrAfter(now, _sequenceTest.currentTrialEndMs)) {
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
    _sequenceTest.primaryValidPatternDtMs = -1;
    _sequenceTest.rejectedInWindowCount = 0;
    _sequenceTest.firstRejectedInWindow = {};
    _sequenceTest.currentTrialHit = false;
    _sequenceTest.currentTrialFinalized = false;
    _sequenceTest.currentTrialUnexpected = 0;
    _sequenceTest.currentTrialRejected = 0;
    _sequenceTest.trialHadAudioOverflow = false;
    _sequenceTest.trialOverflowCountAtStart = _audioSource.stats().overflowCount;
    const detection::AmpDiagnosticSnapshot probeSnapshot = _ampTransientDiagnosticProbe.snapshot();
    _sequenceTest.trialTransientRejectTooShortCountAtStart = probeSnapshot.transientRejectedDurationTooShortCount;
    _sequenceTest.trialTransientRejectTooLongCountAtStart = probeSnapshot.transientRejectedDurationTooLongCount;
    _sequenceTest.trialTransientRejectWeakCountAtStart = probeSnapshot.transientRejectedStrengthTooLowCount;
    _sequenceTest.currentTrialDiagnostics = {};
    _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = _audioSignal.baseline();
    _sequenceTest.currentTrialDiagnostics.runtimePatternCaptured = false;
    _sequenceTest.currentTrialDiagnostics.runtimePatternResult = {};
    _sequenceTest.currentTrialDiagnostics.runtimeFieldState = {};
    _sequenceTest.currentTrialDiagnostics.strongestRejectReason = AmpTransientDetector::TransientRejectReason::None;
    _sequenceTest.currentTrialDiagnostics.strongestRejectDtFromTriggerMs = -1;
    _sequenceTest.currentTrialDiagnostics.strongestRejectDurationMs = 0;
    _sequenceTest.currentTrialDiagnostics.strongestRejectStrength = 0.0f;
    _sequenceTest.nextTriggerAtMs = scheduledAtMs + _sequenceTest.periodMs;

    beginSequenceSampleDump(trialNumber);

    if (!_sequenceTest.externalEmitter) {
        char command[64];
        snprintf(command, sizeof(command), "CHIRP freq=%lu dur=%lu", _sequenceTest.toneHz, _sequenceTest.durationMs);
        sendEmitterCommand(command);
    }
}

void AnalyzerApp::updateSequenceAmbientStats() {
    if (_valMode) {
        return;
    }
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
    } else {
        if (baseline < diagnostics.ambientBaselineMin) {
            diagnostics.ambientBaselineMin = baseline;
        }
        if (baseline > diagnostics.ambientBaselineMax) {
            diagnostics.ambientBaselineMax = baseline;
        }
        if (signalLevel > diagnostics.maxSignalLevel) {
            diagnostics.maxSignalLevel = signalLevel;
        }
    }

    diagnostics.ambientBaselineSamples++;
    diagnostics.ambientBaselineSum += baseline;
}

void AnalyzerApp::finalizeSequenceTrial(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (_sequenceTest.currentTrial == 0) {
        return;
    }

    if (_sequenceTest.currentTrialFinalized) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;

    const detection::AmpDiagnosticSnapshot probeSnapshot = _ampTransientDiagnosticProbe.snapshot();
    diagnostics.peakActiveAtEnd = probeSnapshot.peakActive;
    const char* transientRejectReason = probeSnapshot.transientRejectReason;
    if (strcmp(transientRejectReason, "duration_too_short") == 0) {
        diagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::DurationTooShort;
    } else if (strcmp(transientRejectReason, "duration_too_long") == 0) {
        diagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::DurationTooLong;
    } else if (strcmp(transientRejectReason, "strength_too_low") == 0) {
        diagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::StrengthTooLow;
    } else if (strcmp(transientRejectReason, "peak_still_active") == 0) {
        diagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::PeakStillActive;
    } else {
        diagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::None;
    }
    diagnostics.lastRejectDurationMs = probeSnapshot.rejectedDurationMs;
    diagnostics.lastRejectStrength = probeSnapshot.rejectedStrength;

    const bool invalidAudioTrial = _sequenceTest.trialHadAudioOverflow
                                   || _audioSource.stats().overflowCount != _sequenceTest.trialOverflowCountAtStart;
    const bool hitTrial = !invalidAudioTrial && _sequenceTest.primaryValidPatternCaptured;
    const bool rejectedTrial = !invalidAudioTrial && _sequenceTest.rejectedInWindowCount > 0;
    const bool unexpectedTrial = !invalidAudioTrial && _sequenceTest.currentTrialUnexpected > 0;

    AnalyzerResult result = AnalyzerResult::Miss;
    long dtMs = -1;
    long durMs = -1;
    float strength = 0.0f;

    if (invalidAudioTrial) {
        _sequenceTest.invalidAudio++;
        result = AnalyzerResult::InvalidAudio;
    } else if (hitTrial) {
        _sequenceTest.hits++;
        dtMs = _sequenceTest.primaryValidPatternDtMs;
        durMs = static_cast<long>(_sequenceTest.primaryValidPattern.candidate.durationMs);
        strength = _sequenceTest.primaryValidPattern.candidate.peakStrength;
        if (dtMs >= kLateOnsetMinMs) {
            result = AnalyzerResult::Late;
            _sequenceTest.lateHits++;
        } else {
            result = AnalyzerResult::Expected;
            _sequenceTest.expectedHits++;
        }
        _sequenceTest.totalHitStrengthScaled += static_cast<unsigned long>(_sequenceTest.primaryValidPattern.candidate.peakStrength * 100.0f);
        _sequenceTest.totalHitDurationMs += _sequenceTest.primaryValidPattern.candidate.durationMs;
    } else if (rejectedTrial) {
        result = AnalyzerResult::Rejected;
    } else if (unexpectedTrial) {
        _sequenceTest.unexpected++;
        result = AnalyzerResult::Unexpected;
    } else {
        _sequenceTest.misses++;
    }

    _sequenceTest.duplicates += diagnostics.duplicateCount;
    AnalyzerReport finalizedReport = buildSequenceAnalyzerReport(_sequenceTest.currentTrial, result, dtMs, durMs, strength, invalidAudioTrial, diagnostics.duplicateCount, diagnostics);
    const bool summaryTrial = analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_TRIAL_SUMMARY);
    flushSequenceSampleHistory(now + 1UL);
    printSequenceSampleDump(_sequenceTest.currentTrial);
    if (summaryTrial) {
        printSequenceTrialResult(_sequenceTest.currentTrial, result, dtMs, durMs, strength, invalidAudioTrial, diagnostics.duplicateCount, diagnostics);
    } else {
        printSequenceTrialResult(finalizedReport);
    }
    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_EXPLAIN)) {
        printSequenceExplain(finalizedReport);
    }
    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_CUSTOM) &&
        !analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_EXPLAIN)) {
        printSequenceAmpWindow(finalizedReport);
    }
    if (_sequenceTest.trialReports != nullptr) {
        const size_t reportIndex = static_cast<size_t>(_sequenceTest.currentTrial - 1UL);
        if (reportIndex < _sequenceTest.trialReportCapacity) {
            _sequenceTest.trialReports[reportIndex] = finalizedReport;
            const size_t storedCount = reportIndex + 1UL;
            if (storedCount > _sequenceTest.trialReportCount) {
                _sequenceTest.trialReportCount = storedCount;
            }
        }
    }

    Serial.flush();
    _sequenceTest.currentTrialFinalized = true;

    if (_sequenceTest.currentTrial >= _sequenceTest.totalTrials) {
        printSequenceFinalOutput();
        stopSequenceTest();
    }
}


