#include "../../modes/analyzer/AnalyzerModeApp.h"

#include <Arduino.h>
#include <new>
#include <stdlib.h>
#include <string.h>

#include "../../app/RuntimeDefaults.h"
#include "../../app/TimingUtils.h"
#include "AnalyzerPassRules.h"
#include "../../app/TimeBase.h"
#include "../../app/SerialOut.h"

bool waitForEmitterAck(const char* expectedPrefix, unsigned long timeoutMs);

namespace {

constexpr unsigned long kSequenceWarmupMs = 500;
constexpr long kLateOnsetMinMs = 200L;

unsigned long countSelectedSampleDumpTrials(unsigned long totalTrials, unsigned long firstTrials, unsigned long everyNth) {
    if (totalTrials == 0) {
        return 0;
    }

    // Closed form of "trial <= firstTrials || trial % everyNth == 0" over
    // 1..totalTrials: a loop took minutes for huge trial counts (issue #31).
    const unsigned long first = firstTrials < totalTrials ? firstTrials : totalTrials;
    unsigned long selected = first;
    if (everyNth > 0) {
        selected += totalTrials / everyNth - first / everyNth;
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

void AnalyzerApp::initializeSequenceTest(const PendingSequenceStart& pending) {
    _sequenceTest.active = true;
    _sequenceTest.quiet = pending.quiet || _seqOutputConfig.mode == AnalyzerApp::SeqOutputMode::Quiet;
    _sequenceTest.showDetails = pending.showDetails;
    _sequenceTest.externalEmitter = pending.externalEmitter;
    _sequenceTest.profileKind = pending.profileKind;
    _sequenceTest.outputConfig = _seqOutputConfig;
    _sequenceTest.diagMode = sequenceDiagModeFromOutputWhen(_sequenceTest.outputConfig.when);
    _sequenceTest.progressLineStarted = false;
    _sequenceTest.totalTrials = pending.totalTrials == 0 ? 1 : pending.totalTrials;
    _sequenceTest.periodMs = pending.periodMs == 0 ? 1 : pending.periodMs;
    _sequenceTest.windowStartOffsetMs = 0;
    _sequenceTest.windowEndOffsetMs = pending.windowEndOffsetMs;
    if (_sequenceTest.windowEndOffsetMs < 250) {
        _sequenceTest.windowEndOffsetMs = 250;
    }
    if (_sequenceTest.windowEndOffsetMs >= _sequenceTest.periodMs) {
        _sequenceTest.windowEndOffsetMs = _sequenceTest.periodMs > 250 ? _sequenceTest.periodMs - 250 : _sequenceTest.periodMs;
    }
    // First pulse on to last pulse off; a single chirp when pulses=0.
    const unsigned long emittedSpanMs = pending.pulseCount > 0
        ? pending.pulseCount * pending.durationMs + (pending.pulseCount - 1) * pending.pulseGapMs
        : pending.durationMs;
    if (_sequenceTest.externalEmitter && _sequenceTest.windowEndOffsetMs < emittedSpanMs + 500) {
        _sequenceTest.windowEndOffsetMs = emittedSpanMs + 500;
    }
    if (!_sequenceTest.externalEmitter && emittedSpanMs + 250 > _sequenceTest.windowEndOffsetMs) {
        serialOut().print("SEQ_WARN emitted span close to window end span_ms=");
        serialOut().print(emittedSpanMs);
        serialOut().print(" window=");
        serialOut().println(_sequenceTest.windowEndOffsetMs);
    }
    _sequenceTest.toneHz = pending.toneHz;
    _sequenceTest.durationMs = pending.durationMs;
    _sequenceTest.pulseCount = pending.pulseCount;
    _sequenceTest.pulseGapMs = pending.pulseGapMs;
    _sequenceTest.pulseRampSet = pending.pulseRampSet;
    _sequenceTest.pulseRampMs = pending.pulseRampMs;
    strncpy(_sequenceTest.pulseHop, pending.pulseHop, sizeof(_sequenceTest.pulseHop) - 1);
    _sequenceTest.pulseHop[sizeof(_sequenceTest.pulseHop) - 1] = '\0';
    _sequenceTest.sweepToHz = pending.sweepToHz;
    _sequenceTest.bandLowHz = pending.bandLowHz;
    _sequenceTest.bandHighHz = pending.bandHighHz;
    _sequenceTest.startupDelayMs = pending.startupDelayMs;
    _sequenceTest.reportSettleMs = pending.reportSettleMs == 0 ? 1 : pending.reportSettleMs;
    if (_sequenceTest.externalEmitter &&
        _sequenceTest.windowEndOffsetMs + _sequenceTest.reportSettleMs > _sequenceTest.periodMs) {
        // A trial finalizes reportSettleMs after its window; until then the
        // next emitter chirp is credited to the old trial (issue #31).
        serialOut().print("OBS_WARN window+report_settle>period window=");
        serialOut().print(_sequenceTest.windowEndOffsetMs);
        serialOut().print(" report_settle=");
        serialOut().print(_sequenceTest.reportSettleMs);
        serialOut().print(" period=");
        serialOut().println(_sequenceTest.periodMs);
    }
    _sequenceTest.sampleDumpEnabled = pending.sampleDumpEnabled;
    _sequenceTest.sampleDumpFirstTrials = pending.sampleDumpFirstTrials;
    _sequenceTest.sampleDumpEveryNth = pending.sampleDumpEveryNth;
    _sequenceTest.sampleDumpLeadMs = pending.sampleDumpLeadMs;
    _sequenceTest.sampleDumpTailMs = pending.sampleDumpTailMs;
    _sequenceTest.sampleDumpStepMs = pending.sampleDumpStepMs == 0 ? 1 : pending.sampleDumpStepMs;
    if (_sequenceTest.sampleDumpTailMs < _sequenceTest.sampleDumpLeadMs) {
        _sequenceTest.sampleDumpTailMs = _sequenceTest.sampleDumpLeadMs;
    }
    // sampleMax is the budget over all dumped trials; the per-trial buffer
    // limit is checked separately in startSequenceTest. Clamping it to
    // kMaxSampleRows refused any dump of two default trials (issue #31 B12).
    _sequenceTest.sampleDumpMaxRows = pending.sampleDumpMaxRows == 0 ? 1 : pending.sampleDumpMaxRows;
    _sequenceTest.sampleDumpWarned = false;
    clearSequenceSampleDump();
    const char* setupLabel = pending.setupLabel;
    if (setupLabel != nullptr && setupLabel[0] != '\0') {
        strncpy(_sequenceTest.setupLabel, setupLabel, sizeof(_sequenceTest.setupLabel));
    } else {
        strncpy(_sequenceTest.setupLabel, TEST_SETUP_LABEL, sizeof(_sequenceTest.setupLabel));
    }
    _sequenceTest.setupLabel[sizeof(_sequenceTest.setupLabel) - 1] = '\0';
}

void AnalyzerApp::configureSequenceDetection() {
    const detection::DetectionProfile& selectedProfile = effectiveSequenceProfile();
    _detection.resetState();
    _detection.setVerdictQueueEnabled(false);
    _detection.setScalarTransientConfig(selectedProfile.scalarTransient);
    _detection.setDetectorSelection(selectedProfile.detectorSelection);
    _detection.setInspectionPlan(selectedProfile.inspectionPlan);
    _detection.setFieldStateConfig(selectedProfile.fieldStateConfig);
    _detection.setProfileName(detection::detectionProfileName(selectedProfile.kind));
    _detection.setDiagnosticsEnabled(_sequenceTest.outputConfig.diagnosticsEnabled);
    _sequenceTest.sampleDumpDetectorSelection = selectedProfile.detectorSelection;
    _sequenceTest.sampleDumpObservedStream = selectedProfile.scalarTransient.observedStream;
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(_sequenceTest.toneHz);
    _freqBandStream.setFrequencyUpdateEverySamples(_sequenceTest.outputConfig.frequencyUpdateEverySamples);
    _freqBandStream.setBand(_sequenceTest.bandLowHz, _sequenceTest.bandHighHz);
    _freqBandStream.resetState();
    if (_sequenceTest.bandLowHz != 0) {
        serialOut().print("SEQ band: low_hz=");
        serialOut().print(_sequenceTest.bandLowHz);
        serialOut().print(" high_hz=");
        serialOut().print(_sequenceTest.bandHighHz);
        serialOut().print(" bin_spacing_hz=");
        serialOut().println(_freqBandStream.bandSpacingHz(), 1);
    }
}

void AnalyzerApp::resetSequenceTestRunState() {
    _sequenceTest.currentTrial = 0;
    _sequenceTest.currentTrialScheduledAtMs = 0;
    _sequenceTest.currentTrialStartMs = 0;
    _sequenceTest.currentTrialEndMs = 0;
    _sequenceTest.currentTrialOnsetDetectedMs = 0;
    _sequenceTest.currentTrialPatternDetectedMs = 0;
    _sequenceTest.sourceCandidateCount = 0;
    _sequenceTest.sourceAcceptedCount = 0;
    _sequenceTest.sourceRejectedCount = 0;
    _sequenceTest.inspectedOccurrenceCount = 0;
    _sequenceTest.verdictCount = 0;
    _sequenceTest.primaryValidPatternCaptured = false;
    memset(&_sequenceTest.primaryValidPattern, 0, sizeof(_sequenceTest.primaryValidPattern));
    memset(&_sequenceTest.primaryValidInspectedOccurrence, 0, sizeof(_sequenceTest.primaryValidInspectedOccurrence));
    memset(&_sequenceTest.primaryValidDetectorReport, 0, sizeof(_sequenceTest.primaryValidDetectorReport));
    _sequenceTest.primaryValidPatternDtMs = -1;
    _sequenceTest.primaryAcceptedOccurrenceCaptured = false;
    memset(&_sequenceTest.primaryAcceptedInspectedOccurrence, 0, sizeof(_sequenceTest.primaryAcceptedInspectedOccurrence));
    memset(&_sequenceTest.primaryAcceptedDetectorReport, 0, sizeof(_sequenceTest.primaryAcceptedDetectorReport));
    memset(&_sequenceTest.primaryAcceptedSourceRecord, 0, sizeof(_sequenceTest.primaryAcceptedSourceRecord));
    _sequenceTest.primaryAcceptedOccurrenceDtMs = -1;
    _sequenceTest.rejectedInWindowCount = 0;
    _sequenceTest.bestRejectedPatternCaptured = false;
    memset(&_sequenceTest.bestRejectedInWindow, 0, sizeof(_sequenceTest.bestRejectedInWindow));
    memset(&_sequenceTest.bestRejectedInspectedOccurrence, 0, sizeof(_sequenceTest.bestRejectedInspectedOccurrence));
    memset(&_sequenceTest.bestRejectedDetectorReport, 0, sizeof(_sequenceTest.bestRejectedDetectorReport));
    _sequenceTest.selectedSourceRejectCaptured = false;
    memset(&_sequenceTest.selectedSourceReject, 0, sizeof(_sequenceTest.selectedSourceReject));
    _sequenceTest.consumedSourceRejectEventId = 0;
    _sequenceTest.consumedSourceRejectReportGeneration = 0;
    _sequenceTest.currentTrialFinalized = false;
    _sequenceTest.currentTrialUnexpected = 0;
    _sequenceTest.currentTrialRejected = 0;
    memset(&_sequenceTest.currentTrialDiagnostics, 0, sizeof(_sequenceTest.currentTrialDiagnostics));
    _sequenceTest.trialOverflowCountAtStart = 0;
    _sequenceTest.trialPipelineOverflowCountAtStart = 0;
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
    memset(&_sequenceTest.cleanSummary, 0, sizeof(_sequenceTest.cleanSummary));
}

void AnalyzerApp::startSequenceTest(const PendingSequenceStart& pending) {
    initializeSequenceTest(pending);
    configureSequenceDetection();

    // One history entry per ms holds the lead before the trigger; a longer
    // sampleLead was cut silently (issue #31 R2-19).
    if (_sequenceTest.sampleDumpEnabled && _sequenceTest.sampleDumpLeadMs >= SequenceTest::kMaxSampleHistory) {
        serialOut().print("SAMPLES_WARN reason=lead_beyond_history requested_lead_ms=");
        serialOut().print(_sequenceTest.sampleDumpLeadMs);
        serialOut().print(" max_lead_ms=");
        serialOut().println(static_cast<unsigned long>(SequenceTest::kMaxSampleHistory - 1));
        _sequenceTest.sampleDumpLeadMs = SequenceTest::kMaxSampleHistory - 1;
    }
    if (_sequenceTest.sampleDumpEnabled) {
        const unsigned long selectedTrialsEstimate = countSelectedSampleDumpTrials(
            _sequenceTest.totalTrials, _sequenceTest.sampleDumpFirstTrials, _sequenceTest.sampleDumpEveryNth);
        const unsigned long rowsPerTrial = ((_sequenceTest.sampleDumpLeadMs + _sequenceTest.sampleDumpTailMs) /
                                            _sequenceTest.sampleDumpStepMs) + 1UL;
        const unsigned long requestedRows = selectedTrialsEstimate * rowsPerTrial;
        if (requestedRows > _sequenceTest.sampleDumpMaxRows || rowsPerTrial > SequenceTest::kMaxSampleRows) {
            serialOut().print("SAMPLES_WARN reason=too_many_samples requested=");
            serialOut().print(requestedRows);
            serialOut().print(" max_allowed=");
            serialOut().print(_sequenceTest.sampleDumpMaxRows);
            serialOut().print(" rows_per_trial=");
            serialOut().print(rowsPerTrial);
            serialOut().print(" max_per_trial=");
            serialOut().println(static_cast<unsigned long>(SequenceTest::kMaxSampleRows));
            _sequenceTest.sampleDumpEnabled = false;
        }
    }
    _sequenceTest.startedAtMs = timebase::nowMs();
    _sequenceTest.nextTriggerAtMs = _sequenceTest.startedAtMs + _sequenceTest.startupDelayMs;
    resetSequenceTestRunState();

    if (!_sequenceTest.externalEmitter) {
        // Rebase before the first trial so every run starts from the quiet floor.
        const unsigned long sequenceClaimSendMs = timebase::nowMs();
        sendEmitterCommand("MODE REMOTE");
        const bool sequenceClaimAcked = waitForEmitterAck("OK MODE REMOTE", 1500);
        const unsigned long sequenceClaimAckMs = timebase::nowMs();
        if (_sequenceTest.showDetails && !_sequenceTest.quiet) {
            serialOut().print("SEQ remote claim: send=");
            serialOut().print(sequenceClaimSendMs);
            serialOut().print("ms ack=");
            serialOut().print(sequenceClaimAckMs);
            serialOut().print("ms wait=");
            serialOut().print(sequenceClaimAckMs - sequenceClaimSendMs);
            serialOut().print("ms status=");
            serialOut().println(sequenceClaimAcked ? "ok" : "timeout");
        }
    }

    const unsigned long sequenceRebaseStartMs = timebase::nowMs();
    delay(100);
    if (!_audioSignal.rebase()) {
        serialOut().println("SEQ WARN rebase_timeout no_audio baseline_unchanged");
    }
    if (_sequenceTest.showDetails && !_sequenceTest.quiet) {
        serialOut().print("SEQ rebase: start=");
        serialOut().print(sequenceRebaseStartMs);
        serialOut().print("ms end=");
        serialOut().print(timebase::nowMs());
        serialOut().print("ms elapsed=");
        serialOut().print(timebase::nowMs() - sequenceRebaseStartMs);
        serialOut().println("ms");
    }
    resetAudioSignalState();
    _detection.setDiagnosticsEnabled(_sequenceTest.outputConfig.diagnosticsEnabled);
    _audioSignal.resetStats();
    _audioSource.resetStats();
    if (!_sequenceTest.quiet) {
        serialOut().println("AUDIO stats reset");
    }

    if (!_sequenceTest.quiet) {
        serialOut().print("SEQ start test=");
        serialOut().print(_sequenceTest.setupLabel);
        serialOut().print(" startup_delay_ms=");
        serialOut().print(_sequenceTest.startupDelayMs);
        serialOut().print(" report_settle_ms=");
        serialOut().print(_sequenceTest.reportSettleMs);
        serialOut().print(" loopDelayMs=");
        serialOut().print(TEST_LOOP_DELAY_MS);
        serialOut().print(" logStress=");
        serialOut().println(TEST_LOG_STRESS ? 1 : 0);
    }

    if (_sequenceTest.showDetails && !_sequenceTest.quiet) {
        const detection::DetectionProfile& selectedProfile = effectiveSequenceProfile();
        serialOut().print("SEQ start source=");
        serialOut().print("I2S");
        serialOut().print(" probe=AMP");
        serialOut().print(" profile=");
        serialOut().print(detection::detectionProfileName(_sequenceTest.profileKind));
        serialOut().print(" source=");
        serialOut().print(detection::detectorSelectionName(selectedProfile.detectorSelection));
        serialOut().print(" detector=");
        serialOut().print(detection::detectorSelectionName(selectedProfile.detectorSelection));
        //PARAM TUNING TEMPORARY
        serialOut().print(" scalar_observed_stream=");
        serialOut().print(scalarObservedStreamDisplayName(selectedProfile.scalarTransient.observedStream));
        serialOut().print(" scalar_onset_threshold=");
        serialOut().print(selectedProfile.scalarTransient.onsetDetectionThreshold, 1);
        serialOut().print(" scalar_release_threshold=");
        serialOut().print(selectedProfile.scalarTransient.onsetReleaseThreshold, 1);
        serialOut().print(" scalar_cooldown_ms=");
        serialOut().print(selectedProfile.scalarTransient.cooldownAfterOnsetMs);
        serialOut().print(" scalar_min_duration_ms=");
        serialOut().print(selectedProfile.scalarTransient.minTransientDurationMs);
        serialOut().print(" scalar_max_duration_ms=");
        serialOut().print(selectedProfile.scalarTransient.maxTransientDurationMs);
        serialOut().print(" scalar_min_peak_strength=");
        serialOut().print(selectedProfile.scalarTransient.minTransientPeakStrength, 1);
        serialOut().print(" scalar_release_debounce_ms=");
        serialOut().print(selectedProfile.scalarTransient.releaseDebounceMs);
        serialOut().print(" scalar_require_carrier_quality=");
        serialOut().print(selectedProfile.scalarTransient.requireCarrierQuality ? 1 : 0);
        serialOut().print(" scalar_min_release_coverage_ms=");
        serialOut().print(selectedProfile.scalarTransient.minCoverageAboveReleaseMs);
        serialOut().print(" scalar_min_longest_island_ms=");
        serialOut().print(selectedProfile.scalarTransient.minLongestIslandMs);
        serialOut().print(" scalar_max_gap_ms=");
        serialOut().print(selectedProfile.scalarTransient.maxGapMs);
        serialOut().print(" history.bin_ms=");
        serialOut().print(1);
        serialOut().print(" history.capacity_bins=");
        serialOut().print(detection::FeatureHistory::kBinsPerStream);
        serialOut().print(" history.coverage_ms=");
        serialOut().print(detection::FeatureHistory::kBinsPerStream);
        serialOut().print(" history.stream=amp aggregation=mean_abs+rms+peak_abs");
        serialOut().print(" history.stream=freq_target aggregation=fresh_only");
        serialOut().print(" history.stream=freq_contrast aggregation=fresh_only");
        serialOut().print(" mode=");
        serialOut().print(_sequenceTest.externalEmitter ? "OBS" : "SEQ");
        serialOut().print(" test=");
        serialOut().print(_sequenceTest.setupLabel);
        serialOut().print(" startup_delay_ms=");
        serialOut().print(_sequenceTest.startupDelayMs);
        serialOut().print(" report_settle_ms=");
        serialOut().print(_sequenceTest.reportSettleMs);
        serialOut().print(" loopDelayMs=");
        serialOut().print(TEST_LOOP_DELAY_MS);
        serialOut().print(" logStress=");
        serialOut().print(TEST_LOG_STRESS ? 1 : 0);
        serialOut().print(" quiet=");
        serialOut().print(_sequenceTest.quiet ? 1 : 0);
        serialOut().print(" tries=");
        serialOut().print(_sequenceTest.totalTrials);
        serialOut().print(" period_ms=");
        serialOut().print(_sequenceTest.periodMs);
        serialOut().print(" window_start_ms=");
        serialOut().print(_sequenceTest.windowStartOffsetMs);
        serialOut().print(" window_end_ms=");
        serialOut().print(_sequenceTest.windowEndOffsetMs);
        serialOut().print(" freq_hz=");
        serialOut().print(_sequenceTest.toneHz);
        serialOut().print(" dur_ms=");
        serialOut().print(_sequenceTest.durationMs);
        // Pulse trains only (DET-011); a plain chirp run keeps its old header.
        if (_sequenceTest.pulseCount > 0) {
            serialOut().print(" pulses=");
            serialOut().print(_sequenceTest.pulseCount);
            serialOut().print(" gap_ms=");
            serialOut().print(_sequenceTest.pulseGapMs);
            serialOut().print(" ramp_ms=");
            if (_sequenceTest.pulseRampSet) {
                serialOut().print(_sequenceTest.pulseRampMs);
            } else {
                serialOut().print("default");
            }
        } else if (_sequenceTest.sweepToHz != 0) {
            // Sweep inside the chirp (OUT-005): freq_hz glides to this over dur_ms.
            serialOut().print(" sweep_hz=");
            serialOut().print(_sequenceTest.sweepToHz);
        }
        serialOut().println();
        printDetectionParameters();
        if (!_sequenceTest.quiet) {
            serialOut().println(_sequenceTest.externalEmitter ? "OBS running" : "SEQ running");
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
    _sequenceTest.currentTrialCommandSent = false;
    _sequenceTest.currentTrialCommandSentMs = 0;
    _sequenceTest.currentTrialEndMs = scheduledAtMs + _sequenceTest.windowEndOffsetMs;
    _sequenceTest.currentTrialOnsetDetectedMs = 0;
    _sequenceTest.currentTrialPatternDetectedMs = 0;
    // Trial-local evidence for classifySequenceTrial (issue #31): left
    // cumulative, one rejected candidate made every later miss
    // OccurrenceSeenButRejected.
    _sequenceTest.sourceCandidateCount = 0;
    _sequenceTest.sourceAcceptedCount = 0;
    _sequenceTest.sourceRejectedCount = 0;
    _sequenceTest.inspectedOccurrenceCount = 0;
    _sequenceTest.verdictCount = 0;
    _sequenceTest.primaryValidPatternCaptured = false;
    _sequenceTest.primaryValidPattern = {};
    _sequenceTest.primaryValidInspectedOccurrence = {};
    _sequenceTest.primaryValidDetectorReport = {};
    _sequenceTest.primaryValidPatternDtMs = -1;
    _sequenceTest.primaryAcceptedOccurrenceCaptured = false;
    _sequenceTest.primaryAcceptedInspectedOccurrence = {};
    _sequenceTest.primaryAcceptedDetectorReport = {};
    _sequenceTest.primaryAcceptedSourceRecord = {};
    _sequenceTest.primaryAcceptedOccurrenceDtMs = -1;
    _sequenceTest.rejectedInWindowCount = 0;
    _sequenceTest.bestRejectedPatternCaptured = false;
    _sequenceTest.bestRejectedInWindow = {};
    _sequenceTest.bestRejectedInspectedOccurrence = {};
    _sequenceTest.bestRejectedDetectorReport = {};
    _sequenceTest.selectedSourceRejectCaptured = false;
    _sequenceTest.selectedSourceReject = {};
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
    // Detection-side overflow baselines after the diagnostics reset, which
    // zeroes both counters (issue #31); the I2S baseline above is separate.
    _sequenceTest.trialPipelineOverflowCountAtStart = _detection.pipelineEventOverflowCount();
    _sequenceTest.nextTriggerAtMs = scheduledAtMs + _sequenceTest.periodMs;

    beginSequenceSampleDump(trialNumber);

    if (!_sequenceTest.externalEmitter) {
        char command[128];
        if (_sequenceTest.pulseCount > 0) {
            int written = snprintf(command, sizeof(command), "PULSES %lu %lu %lu trial=%lu freq=%lu",
                                   _sequenceTest.pulseCount, _sequenceTest.durationMs, _sequenceTest.pulseGapMs,
                                   trialNumber, _sequenceTest.toneHz);
            if (_sequenceTest.pulseRampSet && written > 0 && static_cast<size_t>(written) < sizeof(command)) {
                snprintf(command + written, sizeof(command) - written, " ramp=%lu", _sequenceTest.pulseRampMs);
            }
            const size_t used = strlen(command);
            if (_sequenceTest.pulseHop[0] != '\0' && used < sizeof(command)) {
                snprintf(command + used, sizeof(command) - used, " hop=%s", _sequenceTest.pulseHop);
            }
        } else if (_sequenceTest.sweepToHz != 0) {
            snprintf(command, sizeof(command), "CHIRP trial=%lu freq=%lu dur=%lu sweep=%lu", trialNumber,
                     _sequenceTest.toneHz, _sequenceTest.durationMs, _sequenceTest.sweepToHz);
        } else {
            snprintf(command, sizeof(command), "CHIRP trial=%lu freq=%lu dur=%lu", trialNumber, _sequenceTest.toneHz, _sequenceTest.durationMs);
        }
        _sequenceTest.currentTrialCommandSentMs = timebase::nowMs();
        _sequenceTest.currentTrialCommandSent = true;
        sendEmitterCommand(command);
    }
}

void IRAM_ATTR AnalyzerApp::updateSequenceAmbientStats(unsigned long nowMs) {
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
        selection.verdict = &_sequenceTest.primaryValidPattern;
        selection.inspectedOccurrence = _sequenceTest.primaryValidInspectedOccurrence.occurrence.present
            ? &_sequenceTest.primaryValidInspectedOccurrence
            : nullptr;
        selection.detectorReport = _sequenceTest.primaryValidDetectorReport.detectorId != detection::DetectorId::Unknown
            ? &_sequenceTest.primaryValidDetectorReport
            : nullptr;
        selection.sourceEventId = _sequenceTest.primaryAcceptedSourceRecord.eventId;
        selection.sourceReportGeneration = _sequenceTest.primaryAcceptedSourceRecord.reportGeneration;
        selection.sourceEventTrialAttribution = _sequenceTest.primaryAcceptedSourceRecord.eventTrialAttribution;
        selection.occurrenceId = selection.inspectedOccurrence != nullptr
            ? selection.inspectedOccurrence->occurrence.occurrenceId
            : 0UL;
        selection.candidateId = 0;
        selection.reportMatched = detectorReportMatchesOccurrence(selection.detectorReport, selection.inspectedOccurrence);
        selection.dtMs = static_cast<long>(timing::delta(_sequenceTest.primaryValidPattern.primaryStartMs, trialOnsetAnchorMs));
        selection.durationMs = _sequenceTest.primaryValidPattern.primaryDurationMs;
        selection.strength = _sequenceTest.primaryValidPattern.primaryStrength;
        selection.result = selection.dtMs >= kLateOnsetMinMs ? AnalyzerResult::Late : AnalyzerResult::Expected;
        return selection;
    }

    if (_sequenceTest.primaryAcceptedOccurrenceCaptured && _sequenceTest.primaryAcceptedInspectedOccurrence.occurrence.present) {
        selection.kind = SequenceTrialSelection::Kind::AcceptedOccurrence;
        selection.inspectedOccurrence = &_sequenceTest.primaryAcceptedInspectedOccurrence;
        selection.detectorReport = _sequenceTest.primaryAcceptedDetectorReport.detectorId != detection::DetectorId::Unknown
            ? &_sequenceTest.primaryAcceptedDetectorReport
            : nullptr;
        selection.sourceEventId = _sequenceTest.primaryAcceptedSourceRecord.eventId;
        selection.sourceReportGeneration = _sequenceTest.primaryAcceptedSourceRecord.reportGeneration;
        selection.sourceEventTrialAttribution = _sequenceTest.primaryAcceptedSourceRecord.eventTrialAttribution;
        selection.occurrenceId = selection.inspectedOccurrence->occurrence.occurrenceId;
        selection.candidateId = 0;
        selection.reportMatched = detectorReportMatchesOccurrence(selection.detectorReport, selection.inspectedOccurrence);
        selection.dtMs = static_cast<long>(timing::delta(_sequenceTest.primaryAcceptedInspectedOccurrence.occurrence.startMs, trialOnsetAnchorMs));
        selection.durationMs = _sequenceTest.primaryAcceptedInspectedOccurrence.occurrence.durationMs;
        selection.strength = _sequenceTest.primaryAcceptedInspectedOccurrence.occurrence.strength;
        if (_sequenceTest.bestRejectedPatternCaptured &&
            _sequenceTest.bestRejectedInWindow.occurrenceId == selection.occurrenceId) {
            selection.verdict = &_sequenceTest.bestRejectedInWindow;
            selection.dtMs = static_cast<long>(timing::delta(_sequenceTest.bestRejectedInWindow.primaryStartMs, trialOnsetAnchorMs));
            selection.durationMs = _sequenceTest.bestRejectedInWindow.primaryDurationMs;
            selection.strength = _sequenceTest.bestRejectedInWindow.primaryStrength;
        }
        selection.result = selection.dtMs >= kLateOnsetMinMs ? AnalyzerResult::Late : AnalyzerResult::Expected;
        return selection;
    }

    if (_sequenceTest.selectedSourceRejectCaptured &&
        _sequenceTest.selectedSourceReject.detectorReport.detectorId != detection::DetectorId::Unknown &&
        _sequenceTest.selectedSourceReject.eventTrialAttribution == _sequenceTest.currentTrial) {
        selection.kind = SequenceTrialSelection::Kind::RejectedSourceCandidate;
        selection.detectorReport = &_sequenceTest.selectedSourceReject.detectorReport;
        selection.sourceEventId = _sequenceTest.selectedSourceReject.eventId;
        selection.sourceReportGeneration = _sequenceTest.selectedSourceReject.reportGeneration;
        selection.sourceEventTrialAttribution = _sequenceTest.selectedSourceReject.eventTrialAttribution;
        selection.occurrenceId = _sequenceTest.selectedSourceReject.sourceOccurrenceId;
        selection.candidateId = _sequenceTest.selectedSourceReject.sourceCandidateId;
        selection.reportMatched = _sequenceTest.selectedSourceReject.sourceReportMatched;
        selection.dtMs = static_cast<long>(timing::delta(_sequenceTest.selectedSourceReject.detectorReport.selectedReject.startMs, trialOnsetAnchorMs));
        selection.durationMs = _sequenceTest.selectedSourceReject.detectorReport.selectedReject.durationMs;
        selection.strength = _sequenceTest.selectedSourceReject.detectorReport.selectedReject.strength;
        selection.result = AnalyzerResult::Rejected;
        return selection;
    }

    if (_sequenceTest.bestRejectedPatternCaptured) {
        selection.kind = SequenceTrialSelection::Kind::RejectedPattern;
        selection.verdict = &_sequenceTest.bestRejectedInWindow;
        selection.inspectedOccurrence = _sequenceTest.bestRejectedInspectedOccurrence.occurrence.present
            ? &_sequenceTest.bestRejectedInspectedOccurrence
            : nullptr;
        selection.detectorReport = _sequenceTest.bestRejectedDetectorReport.detectorId != detection::DetectorId::Unknown
            ? &_sequenceTest.bestRejectedDetectorReport
            : nullptr;
        selection.occurrenceId = selection.inspectedOccurrence != nullptr
            ? selection.inspectedOccurrence->occurrence.occurrenceId
            : 0UL;
        selection.candidateId = 0;
        selection.reportMatched = detectorReportMatchesOccurrence(selection.detectorReport, selection.inspectedOccurrence);
        selection.dtMs = static_cast<long>(timing::delta(_sequenceTest.bestRejectedInWindow.primaryStartMs, trialOnsetAnchorMs));
        selection.durationMs = _sequenceTest.bestRejectedInWindow.primaryDurationMs;
        selection.strength = _sequenceTest.bestRejectedInWindow.primaryStrength;
        selection.result = AnalyzerResult::Rejected;
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

// The detector-report copies held for a trial date from the event that
// captured them. A weaker scalar reject refreezes the report for its
// rejected_count without a new reject event (decision 2026-10-13, issue #31
// B11), so bring their aggregate counts up to trial end.
void AnalyzerApp::refreshCapturedDetectorReportAggregates() {
    const detection::DetectorReport& live = _detection.activeDetectorReport();
    if (live.detectorId == detection::DetectorId::Unknown) {
        return;
    }
    detection::DetectorReport* const copies[] = {
        &_sequenceTest.primaryValidDetectorReport,
        &_sequenceTest.primaryAcceptedDetectorReport,
        &_sequenceTest.bestRejectedDetectorReport,
        &_sequenceTest.selectedSourceReject.detectorReport,
    };
    for (detection::DetectorReport* copy : copies) {
        if (copy->detectorId == live.detectorId) {
            copy->aggregates = live.aggregates;
        }
    }
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
    refreshCapturedDetectorReportAggregates();
    const uint32_t finalizeStartUs = timebase::nowUs();
    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;

    const bool bufferOverrunTrial = _sequenceTest.bufferOverrun
                                    || _audioSource.stats().overflowCount != _sequenceTest.trialOverflowCountAtStart;
    const long trialOnsetAnchorMs = static_cast<long>(sequenceTrialOnsetAnchorMs());
    const SequenceTrialSelection selectedTrial = selectSequenceTrialSelection(trialOnsetAnchorMs);
    const bool rejectedTrial = _sequenceTest.rejectedInWindowCount > 0;
    const bool unexpectedTrial = selectedTrial.result == AnalyzerResult::Unexpected;

    AnalyzerResult result = selectedTrial.result;
    if (selectedTrial.verdict == nullptr &&
        (selectedTrial.kind == SequenceTrialSelection::Kind::ValidPattern ||
         selectedTrial.kind == SequenceTrialSelection::Kind::AcceptedOccurrence)) {
        result = AnalyzerResult::Miss;
    } else if (selectedTrial.verdict != nullptr && !selectedTrial.verdict->valid) {
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
    // ANA-004 (step 7a): where the per-trial report time goes. One line per
    // trial with diagnostics on: microseconds per print block and the
    // cumulative dropped-DMA count at finalize start (drops caused by this
    // report show up in the next trial's line).
    const uint32_t tBuildUs = timebase::nowUs();
    const uint32_t dropsAtFinalize = _audioSource.stats().droppedDmaBuffers;
    flushSequenceSampleHistory(now + 1UL);
    const uint32_t tFlushUs = timebase::nowUs();
    if (shouldPrintSequenceTrial()) {
        printSequenceTrial(*finalizedReport);
    }
    const uint32_t tTrialUs = timebase::nowUs();
    if (_sequenceTest.sampleDumpEnabled) {
        printSequenceSampleReport(_sequenceTest.currentTrial);
    }
    const uint32_t tSamplesUs = timebase::nowUs();
    if (shouldPrintSequenceInspect(*finalizedReport)) {
        printSequenceInspectCanonical(*finalizedReport);
    }
    const uint32_t tInspectUs = timebase::nowUs();
    if (shouldPrintSequenceSource(*finalizedReport)) {
        printSequenceSourceCanonical(*finalizedReport);
    }
    const uint32_t tSourceUs = timebase::nowUs();
    if (shouldPrintSequenceSystem(*finalizedReport)) {
        printSystemHealth(*finalizedReport);
    }
    const uint32_t tSystemUs = timebase::nowUs();
    if (shouldPrintSequenceDetail(*finalizedReport)) {
        printSequenceDetailCanonical(*finalizedReport);
    }
    const uint32_t tDetailUs = timebase::nowUs();
    if (_sequenceTest.outputConfig.diagnosticsEnabled) {
        serialOut().printf("SEQ_TIMING trial=%lu build_us=%lu flush_us=%lu trial_us=%lu samples_us=%lu inspect_us=%lu source_us=%lu system_us=%lu detail_us=%lu drops_at_finalize=%lu tx_free=%d\n",
                      static_cast<unsigned long>(_sequenceTest.currentTrial),
                      static_cast<unsigned long>(tBuildUs - finalizeStartUs),
                      static_cast<unsigned long>(tFlushUs - tBuildUs),
                      static_cast<unsigned long>(tTrialUs - tFlushUs),
                      static_cast<unsigned long>(tSamplesUs - tTrialUs),
                      static_cast<unsigned long>(tInspectUs - tSamplesUs),
                      static_cast<unsigned long>(tSourceUs - tInspectUs),
                      static_cast<unsigned long>(tSystemUs - tSourceUs),
                      static_cast<unsigned long>(tDetailUs - tSystemUs),
                      static_cast<unsigned long>(dropsAtFinalize),
                      Serial.availableForWrite());
    }
    // OBS keeps the trigger grid: the external emitter runs on its own fixed
    // period, and finalize already waited reportSettleMs past the window, so
    // a second settle here shifted every later window out of phase (issue
    // #31). SEQ commands its own chirps and may push the next one out.
    if (!_sequenceTest.externalEmitter && _sequenceTest.currentTrial < _sequenceTest.totalTrials) {
        const unsigned long settleUntilMs = now + _sequenceTest.reportSettleMs;
        if (timing::beforeDeadline(_sequenceTest.nextTriggerAtMs, settleUntilMs)) {
            _sequenceTest.nextTriggerAtMs = settleUntilMs;
        }
    }
    _sequenceTest.currentTrialFinalized = true;

    if (_sequenceTest.currentTrial >= _sequenceTest.totalTrials) {
        printSequenceSummaryClean();
        stopSequenceTest();
    }

    const unsigned long finalizeUs = static_cast<unsigned long>(timebase::nowUs() - finalizeStartUs);
    if (finalizeUs > _sequenceTest.maxFinalizeTrialUs) {
        _sequenceTest.maxFinalizeTrialUs = finalizeUs;
    }
}

void AnalyzerApp::updateCleanSequenceSummary(const AnalyzerReport& report) {
    AnalyzerCleanSummary& summary = _sequenceTest.cleanSummary;
    summary.profileName = activeAnalyzerProfileName();
    if (summary.detectorId == detection::DetectorId::Unknown) {
        const detection::DetectionProfile& selectedProfile = detection::detectionProfileForKind(_sequenceTest.profileKind);
        (void)selectedProfile;  // one family: the scalar detector
        summary.detectorId = detection::DetectorId::ScalarTransient;
        if (report.detectorReport != nullptr && report.detectorReport->detectorId != detection::DetectorId::Unknown) {
            summary.detectorId = report.detectorReport->detectorId;
        }
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

    if (report.primaryPattern.valid) {
        ++summary.patternValidTrials;
    } else if (detection::analyzer::patternRejectedTrial(report)) {
        ++summary.patternRejectedTrials;
    }

    // Hits only (Expected / Early / Late): a Rejected trial prints dt=na, and
    // a Miss can carry the dt of a settle-period candidate (issue #31 B12,
    // R2-15).
    const AnalyzerResult dtResult = report.classification.result;
    const bool dtCounts = dtResult == AnalyzerResult::Expected
        || dtResult == AnalyzerResult::Early
        || dtResult == AnalyzerResult::Late;
    if (report.classification.dtMs >= 0 && dtCounts) {
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

// dt anchor (ANA-007, step 7a): the time the trial's command was sent when
// this Analyzer drives the Emitter (SEQ); otherwise (OBS, external emitter)
// the EMIT_START marker as polled, else the scheduled trigger. dt from the
// command contains the Emitter's command handling and the D-AMP TX queue, a
// constant (59 ms on 2026-10-22, E2/E3, against 22-38 ms of marker jitter).
unsigned long AnalyzerApp::sequenceTrialOnsetAnchorMs() const {
    if (_sequenceTest.currentTrialCommandSent) {
        return _sequenceTest.currentTrialCommandSentMs;
    }
    if (_sequenceTest.currentTrialDiagnostics.emitStartSeen) {
        return _sequenceTest.currentTrialStartMs + _sequenceTest.currentTrialDiagnostics.emitStartDtMs;
    }
    return _sequenceTest.currentTrialScheduledAtMs;
}

const char* AnalyzerApp::sequenceTrialOnsetAnchorName() const {
    if (_sequenceTest.currentTrialCommandSent) {
        return "command";
    }
    if (_sequenceTest.currentTrialDiagnostics.emitStartSeen) {
        return "marker";
    }
    return "scheduled";
}


