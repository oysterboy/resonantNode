#include "../../detection/analyzer/AnalyzerTrialClassifier.h"
#include "AnalyzerModeApp.h"

#include <Arduino.h>
#include <new>
#include <stdlib.h>
#include <string.h>

#include "../../app/RuntimeDefaults.h"
#include "../../app/TimingUtils.h"
#include "../../detection/analyzer/AnalyzerPassRules.h"
#include "../../app/TimeBase.h"
#include "../../app/SerialOut.h"

#include "tools/AnalyzerEmitterControl.h"

namespace {


unsigned long countSelectedSampleDumpTrials(unsigned long totalTrials, unsigned long firstTrials,
                                            unsigned long everyNth) {
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
               detectorReport->accepted.occurrenceId, inspectedOccurrence->occurrence.occurrenceId,
               detectorReport->accepted.startMs, inspectedOccurrence->occurrence.startMs,
               detectorReport->accepted.endMs, inspectedOccurrence->occurrence.endMs) ||
        detection::analyzer::sourceReportMatchesIdentity(
               detectorReport->selectedReject.occurrenceId, inspectedOccurrence->occurrence.occurrenceId,
               detectorReport->selectedReject.startMs, inspectedOccurrence->occurrence.startMs,
               detectorReport->selectedReject.endMs, inspectedOccurrence->occurrence.endMs);
}

} // namespace

// The run's config, copied once from the accepted start and clamped here; the
// run reads it unchanged afterwards (step 11g C4).
void AnalyzerApp::initializeSequenceTest(const RunConfig& requested) {
    _sequenceTest.active = true;
    RunConfig& config = _sequenceTest.config;
    config = requested;
    config.quiet = requested.quiet || _seqOutputConfig.mode == AnalyzerApp::SeqOutputMode::Quiet;
    config.outputConfig = _seqOutputConfig;  // the running copy of the next-start output settings
    config.totalTrials = requested.totalTrials == 0 ? 1 : requested.totalTrials;
    config.periodMs = requested.periodMs == 0 ? 1 : requested.periodMs;
    if (config.windowEndOffsetMs < 250) {
        config.windowEndOffsetMs = 250;
    }
    if (config.windowEndOffsetMs >= config.periodMs) {
        config.windowEndOffsetMs = config.periodMs > 250 ? config.periodMs - 250 : config.periodMs;
    }
    // First pulse on to last pulse off; a single chirp when pulses=0.
    const unsigned long emittedSpanMs = config.pulseCount > 0
        ? config.pulseCount * config.durationMs + (config.pulseCount - 1) * config.pulseGapMs
        : config.durationMs;
    if (config.externalEmitter && config.windowEndOffsetMs < emittedSpanMs + 500) {
        config.windowEndOffsetMs = emittedSpanMs + 500;
    }
    if (!config.externalEmitter && emittedSpanMs + 250 > config.windowEndOffsetMs) {
        serialOut().print("SEQ_WARN emitted span close to window end span_ms=");
        serialOut().print(emittedSpanMs);
        serialOut().print(" window=");
        serialOut().println(config.windowEndOffsetMs);
    }
    config.reportSettleMs = requested.reportSettleMs == 0 ? 1 : requested.reportSettleMs;
    if (config.externalEmitter && config.windowEndOffsetMs + config.reportSettleMs > config.periodMs) {
        // A trial finalizes reportSettleMs after its window; until then the
        // next emitter chirp is credited to the old trial (issue #31).
        serialOut().print("OBS_WARN window+report_settle>period window=");
        serialOut().print(config.windowEndOffsetMs);
        serialOut().print(" report_settle=");
        serialOut().print(config.reportSettleMs);
        serialOut().print(" period=");
        serialOut().println(config.periodMs);
    }
    config.sampleDumpStepMs = requested.sampleDumpStepMs == 0 ? 1 : requested.sampleDumpStepMs;
    if (config.sampleDumpTailMs < config.sampleDumpLeadMs) {
        config.sampleDumpTailMs = config.sampleDumpLeadMs;
    }
    // sampleMax is the budget over all dumped trials; the per-trial buffer
    // limit is checked separately in startSequenceTest. Clamping it to
    // kMaxSampleRows refused any dump of two default trials (issue #31 B12).
    config.sampleDumpMaxRows = requested.sampleDumpMaxRows == 0 ? 1 : requested.sampleDumpMaxRows;
    _sequenceTest.sampleDump.warned = false;
    clearSequenceSampleDump();
    if (config.setupLabel[0] == '\0') {
        strncpy(config.setupLabel, TEST_SETUP_LABEL, sizeof(config.setupLabel));
    }
    config.setupLabel[sizeof(config.setupLabel) - 1] = '\0';
}

void AnalyzerApp::configureSequenceDetection() {
    // The run's detection.scalar_* values: a PARAM SET during the run changes the next start, and SEQ REPORT /
    // SEQ start print what this run uses (review 12.0 L16).
    _runScalarTuning = _scalarTuning;
    _runScalarTuningCaptured = true;
    const detection::DetectionProfile& selectedProfile = effectiveSequenceProfile();
    _detection.resetState();
    _detection.setVerdictQueueEnabled(false);
    _detection.setScalarTransientConfig(selectedProfile.scalarTransient);
    _detection.setDetectorSelection(selectedProfile.detectorSelection);
    _detection.setInspectionPlan(selectedProfile.inspectionPlan);
    _detection.setFieldStateConfig(selectedProfile.fieldStateConfig);
    _sequenceTest.sampleDump.detectorSelection = selectedProfile.detectorSelection;
    _sequenceTest.sampleDump.observedStream = selectedProfile.scalarTransient.observedStream;
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(_sequenceTest.config.toneHz);
    _freqBandStream.setFrequencyUpdateEverySamples(_sequenceTest.config.outputConfig.frequencyUpdateEverySamples);
    _freqBandStream.setBand(_sequenceTest.config.bandLowHz, _sequenceTest.config.bandHighHz);
    _freqBandStream.resetState();
    if (_sequenceTest.config.bandLowHz != 0) {
        serialOut().print("SEQ band: low_hz=");
        serialOut().print(_sequenceTest.config.bandLowHz);
        serialOut().print(" high_hz=");
        serialOut().print(_sequenceTest.config.bandHighHz);
        serialOut().print(" bin_spacing_hz=");
        serialOut().println(_freqBandStream.bandSpacingHz(), 1);
    }
}

// Run start: no trial yet, the run's tallies at zero (step 11g C4).
void AnalyzerApp::resetSequenceTestRunState() {
    analyzer_seq::resetToDefaults(_sequenceTest.trial);
    analyzer_seq::resetToDefaults(_sequenceTest.totals);
}

void AnalyzerApp::startSequenceTest(const RunConfig& requested) {
    initializeSequenceTest(requested);
    configureSequenceDetection();

    // One history entry per ms holds the lead before the trigger; a longer
    // sampleLead was cut silently (issue #31 R2-19).
    if (_sequenceTest.config.sampleDumpEnabled &&
        _sequenceTest.config.sampleDumpLeadMs >= SampleDumpState::kMaxSampleHistory) {
        serialOut().print("SAMPLES_WARN reason=lead_beyond_history requested_lead_ms=");
        serialOut().print(_sequenceTest.config.sampleDumpLeadMs);
        serialOut().print(" max_lead_ms=");
        serialOut().println(static_cast<unsigned long>(SampleDumpState::kMaxSampleHistory - 1));
        _sequenceTest.config.sampleDumpLeadMs = SampleDumpState::kMaxSampleHistory - 1;
    }
    if (_sequenceTest.config.sampleDumpEnabled) {
        const unsigned long selectedTrialsEstimate =
            countSelectedSampleDumpTrials(_sequenceTest.config.totalTrials, _sequenceTest.config.sampleDumpFirstTrials,
                                          _sequenceTest.config.sampleDumpEveryNth);
        const unsigned long rowsPerTrial =
            ((_sequenceTest.config.sampleDumpLeadMs + _sequenceTest.config.sampleDumpTailMs) /
             _sequenceTest.config.sampleDumpStepMs) +
            1UL;
        const unsigned long requestedRows = selectedTrialsEstimate * rowsPerTrial;
        if (requestedRows > _sequenceTest.config.sampleDumpMaxRows || rowsPerTrial > SampleDumpState::kMaxSampleRows) {
            serialOut().print("SAMPLES_WARN reason=too_many_samples requested=");
            serialOut().print(requestedRows);
            serialOut().print(" max_allowed=");
            serialOut().print(_sequenceTest.config.sampleDumpMaxRows);
            serialOut().print(" rows_per_trial=");
            serialOut().print(rowsPerTrial);
            serialOut().print(" max_per_trial=");
            serialOut().println(static_cast<unsigned long>(SampleDumpState::kMaxSampleRows));
            _sequenceTest.config.sampleDumpEnabled = false;
        }
    }
    _sequenceTest.startedAtMs = timebase::nowMs();
    _sequenceTest.nextTriggerAtMs = _sequenceTest.startedAtMs + _sequenceTest.config.startupDelayMs;
    resetSequenceTestRunState();

    if (!_sequenceTest.config.externalEmitter) {
        // Rebase before the first trial so every run starts from the quiet floor.
        const unsigned long sequenceClaimSendMs = timebase::nowMs();
        sendEmitterCommand("MODE REMOTE");
        const bool sequenceClaimAcked = waitForEmitterAck("OK MODE REMOTE", kEmitterModeAckTimeoutMs);
        const unsigned long sequenceClaimAckMs = timebase::nowMs();
        if (_sequenceTest.config.showDetails && !_sequenceTest.config.quiet) {
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
    if (_sequenceTest.config.showDetails && !_sequenceTest.config.quiet) {
        serialOut().print("SEQ rebase: start=");
        serialOut().print(sequenceRebaseStartMs);
        serialOut().print("ms end=");
        serialOut().print(timebase::nowMs());
        serialOut().print("ms elapsed=");
        serialOut().print(timebase::nowMs() - sequenceRebaseStartMs);
        serialOut().println("ms");
    }
    resetAudioSignalState();
    _audioSignal.resetStats();
    _audioSource.resetStats();
    if (!_sequenceTest.config.quiet) {
        serialOut().println("AUDIO stats reset");
    }

    if (!_sequenceTest.config.quiet) {
        serialOut().print("SEQ start test=");
        serialOut().print(_sequenceTest.config.setupLabel);
        serialOut().print(" startup_delay_ms=");
        serialOut().print(_sequenceTest.config.startupDelayMs);
        serialOut().print(" report_settle_ms=");
        serialOut().print(_sequenceTest.config.reportSettleMs);
        serialOut().print(" loopDelayMs=");
        serialOut().print(TEST_LOOP_DELAY_MS);
        serialOut().print(" logStress=");
        serialOut().println(TEST_LOG_STRESS ? 1 : 0);
    }

    if (_sequenceTest.config.showDetails && !_sequenceTest.config.quiet) {
        const detection::DetectionProfile& selectedProfile = effectiveSequenceProfile();
        serialOut().print("SEQ start source=");
        serialOut().print("I2S");
        serialOut().print(" probe=AMP");
        serialOut().print(" profile=");
        serialOut().print(detection::detectionProfileName(_sequenceTest.config.profileKind));
        serialOut().print(" source=");
        serialOut().print(detection::detectorSelectionName(selectedProfile.detectorSelection));
        serialOut().print(" detector=");
        serialOut().print(detection::detectorSelectionName(selectedProfile.detectorSelection));
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
        serialOut().print(_sequenceTest.config.externalEmitter ? "OBS" : "SEQ");
        serialOut().print(" test=");
        serialOut().print(_sequenceTest.config.setupLabel);
        serialOut().print(" startup_delay_ms=");
        serialOut().print(_sequenceTest.config.startupDelayMs);
        serialOut().print(" report_settle_ms=");
        serialOut().print(_sequenceTest.config.reportSettleMs);
        serialOut().print(" loopDelayMs=");
        serialOut().print(TEST_LOOP_DELAY_MS);
        serialOut().print(" logStress=");
        serialOut().print(TEST_LOG_STRESS ? 1 : 0);
        serialOut().print(" quiet=");
        serialOut().print(_sequenceTest.config.quiet ? 1 : 0);
        serialOut().print(" tries=");
        serialOut().print(_sequenceTest.config.totalTrials);
        serialOut().print(" period_ms=");
        serialOut().print(_sequenceTest.config.periodMs);
        serialOut().print(" window_start_ms=");
        serialOut().print(0UL);
        serialOut().print(" window_end_ms=");
        serialOut().print(_sequenceTest.config.windowEndOffsetMs);
        serialOut().print(" freq_hz=");
        serialOut().print(_sequenceTest.config.toneHz);
        serialOut().print(" dur_ms=");
        serialOut().print(_sequenceTest.config.durationMs);
        // Pulse trains only (DET-011); a plain chirp run keeps its old header.
        if (_sequenceTest.config.pulseCount > 0) {
            serialOut().print(" pulses=");
            serialOut().print(_sequenceTest.config.pulseCount);
            serialOut().print(" gap_ms=");
            serialOut().print(_sequenceTest.config.pulseGapMs);
            serialOut().print(" ramp_ms=");
            if (_sequenceTest.config.pulseRampSet) {
                serialOut().print(_sequenceTest.config.pulseRampMs);
            } else {
                serialOut().print("default");
            }
        } else if (_sequenceTest.config.sweepToHz != 0) {
            // Sweep inside the chirp (OUT-005): freq_hz glides to this over dur_ms.
            serialOut().print(" sweep_hz=");
            serialOut().print(_sequenceTest.config.sweepToHz);
        }
        serialOut().println();
        printDetectionParameters();
        if (!_sequenceTest.config.quiet) {
            serialOut().println(_sequenceTest.config.externalEmitter ? "OBS running" : "SEQ running");
        }
    }
}

void AnalyzerApp::stopSequenceTest() {
    _sequenceTest.active = false;
    _sequenceTest.sampleDump.capturing = false;
}

void AnalyzerApp::updateSequenceTest(unsigned long now) {
    if (!_sequenceTest.active) {
        return;
    }

    if (_sequenceTest.trial.number > 0 && !_sequenceTest.trial.finalized) {
        const unsigned long finalizeAtMs = _sequenceTest.trial.endMs + _sequenceTest.config.reportSettleMs;
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

    if (_sequenceTest.trial.number >= _sequenceTest.config.totalTrials) {
        return;
    }

    const unsigned long trialNumber = _sequenceTest.trial.number + 1;
    const unsigned long scheduledAtMs = _sequenceTest.nextTriggerAtMs;
    // One reset to defaults for the whole trial (TrialState{}): its window, evidence, diagnostics and loop timing.
    // Trial-local evidence for classifySequenceTrial (issue #31): left
    // cumulative, one rejected candidate made every later miss
    // OccurrenceSeenButRejected.
    analyzer_seq::resetToDefaults(_sequenceTest.trial);
    _sequenceTest.trial.number = trialNumber;
    _sequenceTest.trial.scheduledAtMs = scheduledAtMs;
    _sequenceTest.trial.startMs = scheduledAtMs;
    _sequenceTest.trial.endMs = scheduledAtMs + _sequenceTest.config.windowEndOffsetMs;
    _sequenceTest.trial.overflowCountAtStart = _audioSource.stats().overflowCount;
    _sequenceTest.trial.diagnostics.acceptedAmbientBaseline = _audioSignal.baseline();
    _detection.resetSourceRejectSummaries();
    resetLoopHealthWindow();
    printSequenceTrialHeader(trialNumber);
    if (_sequenceTest.config.outputConfig.diagnosticsEnabled) {
        _detection.resetDiagnosticsCounters();
    }
    // Detection-side overflow baselines after the diagnostics reset, which
    // zeroes both counters (issue #31); the I2S baseline above is separate.
    _sequenceTest.trial.pipelineOverflowCountAtStart = _detection.diagnostics().pipelineEventOverflowCount();
    _sequenceTest.nextTriggerAtMs = scheduledAtMs + _sequenceTest.config.periodMs;

    beginSequenceSampleDump(trialNumber);

    if (!_sequenceTest.config.externalEmitter) {
        char command[128];
        if (_sequenceTest.config.pulseCount > 0) {
            int written = snprintf(command, sizeof(command), "PULSES %lu %lu %lu trial=%lu freq=%lu",
                                   _sequenceTest.config.pulseCount, _sequenceTest.config.durationMs,
                                   _sequenceTest.config.pulseGapMs, trialNumber, _sequenceTest.config.toneHz);
            if (_sequenceTest.config.pulseRampSet && written > 0 && static_cast<size_t>(written) < sizeof(command)) {
                snprintf(command + written, sizeof(command) - written, " ramp=%lu", _sequenceTest.config.pulseRampMs);
            }
            const size_t used = strlen(command);
            if (_sequenceTest.config.pulseHop[0] != '\0' && used < sizeof(command)) {
                snprintf(command + used, sizeof(command) - used, " hop=%s", _sequenceTest.config.pulseHop);
            }
        } else if (_sequenceTest.config.sweepToHz != 0) {
            snprintf(command, sizeof(command), "CHIRP trial=%lu freq=%lu dur=%lu sweep=%lu", trialNumber,
                     _sequenceTest.config.toneHz, _sequenceTest.config.durationMs, _sequenceTest.config.sweepToHz);
        } else {
            snprintf(command, sizeof(command), "CHIRP trial=%lu freq=%lu dur=%lu", trialNumber,
                     _sequenceTest.config.toneHz, _sequenceTest.config.durationMs);
        }
        _sequenceTest.trial.commandSentMs = timebase::nowMs();
        _sequenceTest.trial.commandSent = true;
        sendEmitterCommand(command);
    }
}

void IRAM_ATTR AnalyzerApp::updateSequenceAmbientStats(unsigned long nowMs) {
    if (!_sequenceTest.active || _sequenceTest.trial.number == 0 || _sequenceTest.trial.finalized) {
        return;
    }

    auto& diagnostics = _sequenceTest.trial.diagnostics;
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
}

AnalyzerApp::SequenceTrialSelection AnalyzerApp::selectSequenceTrialSelection(unsigned long trialOnsetAnchorMs) const {
    SequenceTrialSelection selection = {};

    if (_sequenceTest.trial.primaryValidPatternCaptured) {
        selection.kind = SequenceTrialSelection::Kind::ValidPattern;
        selection.verdict = &_sequenceTest.trial.primaryValidPattern;
        selection.inspectedOccurrence = _sequenceTest.trial.primaryValidInspectedOccurrence.occurrence.present
            ? &_sequenceTest.trial.primaryValidInspectedOccurrence
            : nullptr;
        selection.detectorReport =
            _sequenceTest.trial.primaryValidDetectorReport.detectorId != detection::DetectorId::Unknown
            ? &_sequenceTest.trial.primaryValidDetectorReport
            : nullptr;
        // The valid verdict's own event (review 12.0 L12); the first accepted one only when it carried no record.
        const detection::SourceDiagnosticRecord& source = _sequenceTest.trial.primaryValidSourceRecordCaptured
            ? _sequenceTest.trial.primaryValidSourceRecord
            : _sequenceTest.trial.primaryAcceptedSourceRecord;
        selection.sourceEventId = source.eventId;
        selection.sourceReportGeneration = source.reportGeneration;
        selection.sourceEventTrialAttribution = source.eventTrialAttribution;
        selection.occurrenceId =
            selection.inspectedOccurrence != nullptr ? selection.inspectedOccurrence->occurrence.occurrenceId : 0UL;
        selection.candidateId = 0;
        selection.reportMatched =
            detectorReportMatchesOccurrence(selection.detectorReport, selection.inspectedOccurrence);
        selection.dtMs = static_cast<long>(
            timing::delta(_sequenceTest.trial.primaryValidPattern.primaryStartMs, trialOnsetAnchorMs));
        selection.durationMs = _sequenceTest.trial.primaryValidPattern.primaryDurationMs;
        selection.strength = _sequenceTest.trial.primaryValidPattern.primaryStrength;
        selection.result = selection.dtMs >= kAnalyzerLateOnsetMinMs ? AnalyzerResult::Late : AnalyzerResult::Expected;
        return selection;
    }

    if (_sequenceTest.trial.primaryAcceptedOccurrenceCaptured &&
        _sequenceTest.trial.primaryAcceptedInspectedOccurrence.occurrence.present) {
        selection.kind = SequenceTrialSelection::Kind::AcceptedOccurrence;
        selection.inspectedOccurrence = &_sequenceTest.trial.primaryAcceptedInspectedOccurrence;
        selection.detectorReport =
            _sequenceTest.trial.primaryAcceptedDetectorReport.detectorId != detection::DetectorId::Unknown
            ? &_sequenceTest.trial.primaryAcceptedDetectorReport
            : nullptr;
        selection.sourceEventId = _sequenceTest.trial.primaryAcceptedSourceRecord.eventId;
        selection.sourceReportGeneration = _sequenceTest.trial.primaryAcceptedSourceRecord.reportGeneration;
        selection.sourceEventTrialAttribution = _sequenceTest.trial.primaryAcceptedSourceRecord.eventTrialAttribution;
        selection.occurrenceId = selection.inspectedOccurrence->occurrence.occurrenceId;
        selection.candidateId = 0;
        selection.reportMatched =
            detectorReportMatchesOccurrence(selection.detectorReport, selection.inspectedOccurrence);
        selection.dtMs = static_cast<long>(timing::delta(
            _sequenceTest.trial.primaryAcceptedInspectedOccurrence.occurrence.startMs, trialOnsetAnchorMs));
        selection.durationMs = _sequenceTest.trial.primaryAcceptedInspectedOccurrence.occurrence.durationMs;
        selection.strength = _sequenceTest.trial.primaryAcceptedInspectedOccurrence.occurrence.strength;
        // The accepted occurrence's own verdict (review 12.0 M5): with two invalid verdicts in the window the
        // strongest rejected one may be the other, and the trial read missing_evaluator_report.
        if (_sequenceTest.trial.primaryAcceptedVerdictCaptured) {
            const detection::OccurrenceVerdict& own = _sequenceTest.trial.primaryAcceptedVerdict;
            selection.verdict = &own;
            selection.dtMs = static_cast<long>(timing::delta(own.primaryStartMs, trialOnsetAnchorMs));
            selection.durationMs = own.primaryDurationMs;
            selection.strength = own.primaryStrength;
        }
        selection.result = selection.dtMs >= kAnalyzerLateOnsetMinMs ? AnalyzerResult::Late : AnalyzerResult::Expected;
        return selection;
    }

    if (_sequenceTest.trial.selectedSourceRejectCaptured &&
        _sequenceTest.trial.selectedSourceReject.detectorReport.detectorId != detection::DetectorId::Unknown &&
        _sequenceTest.trial.selectedSourceReject.eventTrialAttribution == _sequenceTest.trial.number) {
        selection.kind = SequenceTrialSelection::Kind::RejectedSourceCandidate;
        selection.detectorReport = &_sequenceTest.trial.selectedSourceReject.detectorReport;
        selection.sourceEventId = _sequenceTest.trial.selectedSourceReject.eventId;
        selection.sourceReportGeneration = _sequenceTest.trial.selectedSourceReject.reportGeneration;
        selection.sourceEventTrialAttribution = _sequenceTest.trial.selectedSourceReject.eventTrialAttribution;
        selection.occurrenceId = _sequenceTest.trial.selectedSourceReject.sourceOccurrenceId;
        selection.candidateId = _sequenceTest.trial.selectedSourceReject.sourceCandidateId;
        selection.reportMatched = _sequenceTest.trial.selectedSourceReject.sourceReportMatched;
        selection.dtMs = static_cast<long>(timing::delta(
            _sequenceTest.trial.selectedSourceReject.detectorReport.selectedReject.startMs, trialOnsetAnchorMs));
        selection.durationMs = _sequenceTest.trial.selectedSourceReject.detectorReport.selectedReject.durationMs;
        selection.strength = _sequenceTest.trial.selectedSourceReject.detectorReport.selectedReject.strength;
        selection.result = AnalyzerResult::Rejected;
        return selection;
    }

    if (_sequenceTest.trial.bestRejectedPatternCaptured) {
        selection.kind = SequenceTrialSelection::Kind::RejectedPattern;
        selection.verdict = &_sequenceTest.trial.bestRejectedInWindow;
        selection.inspectedOccurrence = _sequenceTest.trial.bestRejectedInspectedOccurrence.occurrence.present
            ? &_sequenceTest.trial.bestRejectedInspectedOccurrence
            : nullptr;
        selection.detectorReport =
            _sequenceTest.trial.bestRejectedDetectorReport.detectorId != detection::DetectorId::Unknown
            ? &_sequenceTest.trial.bestRejectedDetectorReport
            : nullptr;
        selection.occurrenceId =
            selection.inspectedOccurrence != nullptr ? selection.inspectedOccurrence->occurrence.occurrenceId : 0UL;
        selection.candidateId = 0;
        selection.reportMatched =
            detectorReportMatchesOccurrence(selection.detectorReport, selection.inspectedOccurrence);
        selection.dtMs = static_cast<long>(
            timing::delta(_sequenceTest.trial.bestRejectedInWindow.primaryStartMs, trialOnsetAnchorMs));
        selection.durationMs = _sequenceTest.trial.bestRejectedInWindow.primaryDurationMs;
        selection.strength = _sequenceTest.trial.bestRejectedInWindow.primaryStrength;
        selection.result = AnalyzerResult::Rejected;
        return selection;
    }

    if (_sequenceTest.trial.unexpected > 0) {
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
    const detection::DetectorReport& live = _detection.diagnostics().activeDetectorReport();
    if (live.detectorId == detection::DetectorId::Unknown) {
        return;
    }
    detection::DetectorReport* const copies[] = {
        &_sequenceTest.trial.primaryValidDetectorReport,
        &_sequenceTest.trial.primaryAcceptedDetectorReport,
        &_sequenceTest.trial.bestRejectedDetectorReport,
        &_sequenceTest.trial.selectedSourceReject.detectorReport,
    };
    for (detection::DetectorReport* copy : copies) {
        if (copy->detectorId == live.detectorId) {
            copy->aggregates = live.aggregates;
        }
    }
}

void AnalyzerApp::finalizeSequenceTrial(unsigned long now) {
    if (_sequenceTest.trial.number == 0) {
        return;
    }

    if (_sequenceTest.trial.finalized) {
        return;
    }

    // Flush late emitter markers before snapshotting the trial.
    pollEmitterSerial();
    refreshCapturedDetectorReportAggregates();
    const uint32_t finalizeStartUs = timebase::nowUs();
    auto& diagnostics = _sequenceTest.trial.diagnostics;

    const bool bufferOverrunTrial = _sequenceTest.trial.bufferOverrun ||
        _audioSource.stats().overflowCount != _sequenceTest.trial.overflowCountAtStart;
    const long trialOnsetAnchorMs = static_cast<long>(sequenceTrialOnsetAnchorMs());
    const SequenceTrialSelection selectedTrial = selectSequenceTrialSelection(trialOnsetAnchorMs);
    const bool rejectedInWindow = _sequenceTest.trial.rejectedInWindowCount > 0;
    long dtMs = selectedTrial.dtMs;
    long durMs = static_cast<long>(selectedTrial.durationMs);
    float strength = selectedTrial.strength;

    // The trial result is decided once, by classifySequenceTrial inside the report builder (11g C5).
    AnalyzerReport* finalizedReport = sequenceReportScratch();
    buildSequenceAnalyzerReport(*finalizedReport, selectedTrial, _sequenceTest.trial.number, rejectedInWindow, dtMs,
                                durMs, strength, bufferOverrunTrial, diagnostics.duplicateCount, diagnostics);
    updateCleanSequenceSummary(*finalizedReport);
    _sequenceTest.totals.completedTrials++;
    flushSequenceSampleHistory(now + 1UL);
    if (shouldPrintSequenceTrial()) {
        printSequenceTrial(*finalizedReport);
    }
    if (_sequenceTest.config.sampleDumpEnabled) {
        printSequenceSampleReport(_sequenceTest.trial.number);
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
    // OBS keeps the trigger grid: the external emitter runs on its own fixed
    // period, and finalize already waited reportSettleMs past the window, so
    // a second settle here shifted every later window out of phase (issue
    // #31). SEQ commands its own chirps and may push the next one out.
    if (!_sequenceTest.config.externalEmitter && _sequenceTest.trial.number < _sequenceTest.config.totalTrials) {
        const unsigned long settleUntilMs = now + _sequenceTest.config.reportSettleMs;
        if (timing::beforeDeadline(_sequenceTest.nextTriggerAtMs, settleUntilMs)) {
            _sequenceTest.nextTriggerAtMs = settleUntilMs;
        }
    }
    _sequenceTest.trial.finalized = true;

    if (_sequenceTest.trial.number >= _sequenceTest.config.totalTrials) {
        printSequenceSummaryClean();
        stopSequenceTest();
    }

    const unsigned long finalizeUs = static_cast<unsigned long>(timebase::nowUs() - finalizeStartUs);
    if (finalizeUs > _sequenceTest.totals.maxFinalizeTrialUs) {
        _sequenceTest.totals.maxFinalizeTrialUs = finalizeUs;
    }
}

void AnalyzerApp::updateCleanSequenceSummary(const AnalyzerReport& report) {
    AnalyzerCleanSummary& summary = _sequenceTest.totals.cleanSummary;
    summary.profileName = activeAnalyzerProfileName();
    if (summary.detectorId == detection::DetectorId::Unknown) {
        // One family: the scalar detector.
        summary.detectorId = detection::DetectorId::ScalarTransient;
        if (report.detectorReport != nullptr && report.detectorReport->detectorId != detection::DetectorId::Unknown) {
            summary.detectorId = report.detectorReport->detectorId;
        }
    }
    summary.trials = static_cast<unsigned int>(_sequenceTest.config.totalTrials);

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
    const bool dtCounts =
        dtResult == AnalyzerResult::Expected || dtResult == AnalyzerResult::Early || dtResult == AnalyzerResult::Late;
    if (report.classification.dtMs >= 0 && dtCounts) {
        summary.totalDtMs += report.classification.dtMs;
        ++summary.dtCount;
    }

    if (report.occurrences.primaryStrength > 0.0f) {
        summary.totalStrength += report.occurrences.primaryStrength;
        ++summary.strengthCount;
    }

    summary.completed = detection::analyzer::completedPrimaryTrialCount(summary);
}

// dt anchor (ANA-007, step 7a): the time the trial's command was sent when
// this Analyzer drives the Emitter (SEQ); otherwise (OBS, external emitter)
// the EMIT_START marker as polled, else the scheduled trigger. The marker
// branch only fires when an Emitter on the link prints markers for this
// trial's number during OBS, which no current setup does (review 12.0 L18):
// in practice OBS anchors on the scheduled trigger. dt from the
// command contains the Emitter's command handling and the D-AMP TX queue, a
// constant (59 ms on 2026-10-22, E2/E3, against 22-38 ms of marker jitter).
unsigned long AnalyzerApp::sequenceTrialOnsetAnchorMs() const {
    if (_sequenceTest.trial.commandSent) {
        return _sequenceTest.trial.commandSentMs;
    }
    if (_sequenceTest.trial.diagnostics.emitStartSeen) {
        return _sequenceTest.trial.startMs + _sequenceTest.trial.diagnostics.emitStartDtMs;
    }
    return _sequenceTest.trial.scheduledAtMs;
}
