#include "AnalyzerSystemReporter.h"

#include "../../../modes/analyzer/AnalyzerModeApp.h"
#include "AnalyzerRawHealth.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "../../../app/TimeBase.h"
#include "../../../app/TimingUtils.h"
#include "../../../app/SerialOut.h"

void AnalyzerApp::printSystemHealth(const AnalyzerReport& report) const {
    const AudioSourceStats& sourceStats = _audioSource.stats();
    const AudioSignalStats& signalStats = _audioSignal.stats();
    const unsigned long sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const bool compactHealth = _sequenceTest.outputConfig.verbosity == 0U;
    const bool hasTrialWindow = _sequenceTest.active
        && _sequenceTest.currentTrial > 0
        && timing::atOrAfter(_sequenceTest.currentTrialEndMs, _sequenceTest.currentTrialStartMs);
    const unsigned long windowStartMs = hasTrialWindow
        ? _sequenceTest.currentTrialStartMs
        : activeRunStartMs();
    const unsigned long windowEndMs = hasTrialWindow
        ? _sequenceTest.currentTrialEndMs
        : activeRunEndMs();
    const unsigned long windowElapsedMs = windowStartMs > 0 ? timing::elapsedOrZero(windowEndMs, windowStartMs) : 0UL;
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

    serialOut().print("SYSTEM_HEALTH");
    serialOut().print(" loop_avg_us=");
    serialOut().print(loopAvgUs);
    serialOut().print(" update_loop_avg_us=");
    serialOut().print(updateLoopAvgUs);
    if (!compactHealth) {
        serialOut().print(" trial=");
        serialOut().print(report.context.trial);
        serialOut().print(" t_ms=");
        serialOut().print(report.context.timestampMs);
        serialOut().print(" uptime_ms=");
        serialOut().print(timebase::nowMs());
        serialOut().print(" boot_count=");
        serialOut().print(analyzerBootCount());
        serialOut().print(" reset_reason=");
        serialOut().print(currentResetReasonName());
        serialOut().print(" window_start_ms=");
        serialOut().print(windowStartMs);
        serialOut().print(" window_end_ms=");
        serialOut().print(windowEndMs);
        serialOut().print(" window_elapsed_ms=");
        serialOut().print(windowElapsedMs);
        serialOut().print(" free_heap=");
        serialOut().print(esp_get_free_heap_size());
        serialOut().print(" loop_stats_window_ms=");
        serialOut().print(windowElapsedMs);
        serialOut().print(" loop_stats_scope=");
        serialOut().print(_sequenceTest.active && _sequenceTest.currentTrialStartMs > 0 ? "trial_window" : "run_window");
        serialOut().print(" loop_count=");
        serialOut().print(_loopHealth.count);
        serialOut().print(" loop_max_window_us=");
        serialOut().print(_loopHealth.maxUs);
        serialOut().print(" loop_over_5ms=");
        serialOut().print(_loopHealth.over5ms);
        serialOut().print(" loop_over_20ms=");
        serialOut().print(_loopHealth.over20ms);
        serialOut().print(" loop_max_since_boot_us=");
        serialOut().print(_loopMaxSinceBootUs);
        serialOut().print(" update_loop_count=");
        serialOut().print(_sequenceTest.updateLoopCount);
        serialOut().print(" update_loop_max_us=");
        serialOut().print(_sequenceTest.currentTrialUpdateLoopMaxUs);
        serialOut().print(" sample_work_max_us=");
        serialOut().print(_sequenceTest.maxSampleWorkUs);
        serialOut().print(" trial_finalize_max_us=");
        serialOut().print(_sequenceTest.maxFinalizeTrialUs);
        serialOut().print(" detector_observe_count=");
        serialOut().print(_detection.observeFrameCount());
        serialOut().print(" detector_fresh_input_count=");
        serialOut().print(_detection.freshDetectorInputCount());
        serialOut().print(" detector_drain_count=");
        serialOut().print(_detection.detectorDrainCount());
        serialOut().print(" evaluator_drain_count=");
        serialOut().print(_detection.evaluatorDrainCount());
        serialOut().print(" detector_report_refresh_count=");
        serialOut().print(_detection.detectorReportRefreshCount());
        serialOut().print(" detector_report_mismatch_count=");
        serialOut().print(_detection.detectorReportMismatchCount());
        serialOut().print(" no_fresh_frequency_skip_count=");
        serialOut().print(_detection.noFreshFrequencySkipCount());
        serialOut().print(" detector_occurrence_popped_count=");
        serialOut().print(_detection.detectorOccurrencePoppedCount());
        serialOut().print(" detector_valid_occurrence_popped_count=");
        serialOut().print(_detection.detectorValidOccurrencePoppedCount());
        serialOut().print(" evaluator_accept_attempt_count=");
        serialOut().print(_detection.evaluatorAcceptAttemptCount());
        serialOut().print(" evaluator_accept_success_count=");
        serialOut().print(_detection.evaluatorAcceptSuccessCount());
        serialOut().print(" evaluator_accept_reject_count=");
        serialOut().print(_detection.evaluatorAcceptRejectCount());
        serialOut().print(" verdict_produced_count=");
        serialOut().print(_detection.verdictProducedCount());
        serialOut().print(" verdict_event_pushed_count=");
        serialOut().print(_detection.verdictEventPushedCount());
        serialOut().print(" verdict_event_dropped_count=");
        serialOut().print(_detection.verdictEventDroppedCount());
        serialOut().print(" evaluator_latest_reject_reason=");
        serialOut().print(detection::evaluatorInputRejectReasonName(_detection.latestEvaluatorInputRejectReason()));
    }
    serialOut().println();
    _loopHealth.reset();

    serialOut().print("AUDIO_IO_HEALTH");
    serialOut().print(" processed_ratio=");
    serialOut().print(processedRatio, 3);
    if (!compactHealth) {
        serialOut().print(" t_ms=");
        serialOut().print(report.context.timestampMs);
        serialOut().print(" i2s_reads=");
        serialOut().print(sourceStats.reads);
        serialOut().print(" i2s_bytes_read=");
        serialOut().print(sourceStats.readBytes);
        serialOut().print(" processed_samples=");
        serialOut().print(processedSamples);
        serialOut().print(" expected_samples=");
        serialOut().print(expectedSamples);
        serialOut().print(" frames_emitted=");
        serialOut().print(signalStats.blocksProcessed);
        serialOut().print(" partial_frames=");
        serialOut().print(sourceStats.shortReads);
        serialOut().print(" duplicate_frames=0");
        serialOut().print(" gap_samples=0");
        serialOut().print(" overlap_samples=0");
        serialOut().print(" max_block_age_ms=");
        serialOut().print(_sequenceTest.active ? _sequenceTest.maxBlockAgeMs : 0UL);
        serialOut().print(" max_processing_lag_ms=");
        serialOut().println(_sequenceTest.active ? _sequenceTest.maxProcessingLagMs : 0UL);
    } else {
        serialOut().println();
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
        serialOut().print("RAW_AUDIO_HEALTH");
        serialOut().print(" verdict=");
        serialOut().print(rawHealth != nullptr ? rawHealth : "unknown");
    }
    if (_sequenceTest.outputConfig.verbosity >= 2U) {
        serialOut().print(" t_ms=");
        serialOut().print(report.context.timestampMs);
        serialOut().print(" frames=");
        serialOut().print(rawDiagnostics.rawFrames);
        serialOut().print(" min_sample=");
        serialOut().print(rawDiagnostics.rawFrames > 0 ? rawDiagnostics.rawMin : 0);
        serialOut().print(" max_sample=");
        serialOut().print(rawDiagnostics.rawFrames > 0 ? rawDiagnostics.rawMax : 0);
        serialOut().print(" dc_mean=");
        serialOut().print(rawDcMean, 1);
        serialOut().print(" mean_abs=");
        serialOut().print(rawMeanAbs, 1);
        serialOut().print(" max_abs=");
        serialOut().print(rawMaxAbs);
        serialOut().print(" zero_ratio=");
        serialOut().print(rawZeroRatio, 3);
        serialOut().print(" clip_count=");
        serialOut().print(_sequenceTest.currentTrialDiagnostics.audioRmsTooHighFrames);
        serialOut().print(" flatline=");
        serialOut().println(rawHealth != nullptr && strcmp(rawHealth, "flatline") == 0 ? 1 : 0);
    } else if (_sequenceTest.outputConfig.verbosity >= 1U) {
        serialOut().println();
    }

    if (!shouldPrintHardwareDiagnostics()) {
        return;
    }

    serialOut().print("I2S_IO_HEALTH");
    serialOut().print(" reads=");
    serialOut().print(sourceStats.reads);
    serialOut().print(" bytes=");
    serialOut().print(sourceStats.readBytes);
    serialOut().print(" zero_reads=");
    serialOut().print(sourceStats.zeroReads);
    serialOut().print(" short_reads=");
    serialOut().print(sourceStats.shortReads);
    serialOut().print(" max_read_bytes=");
    serialOut().print(sourceStats.maxReadBytes);
    serialOut().print(" read_errors=");
    serialOut().print(sourceStats.readErrors);
    serialOut().print(" overflow=");
    serialOut().print(sourceStats.overflowCount);
    serialOut().print(" total_samples=");
    serialOut().println(static_cast<unsigned long long>(sourceStats.totalSamplesRead));
}
