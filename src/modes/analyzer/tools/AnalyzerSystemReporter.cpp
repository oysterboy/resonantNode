#include "AnalyzerSystemReporter.h"

#include "../AnalyzerModeApp.h"
#include "../../../detection/analyzer/tools/AnalyzerRawHealth.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "../../../app/TimeBase.h"
#include "../../../app/TimingUtils.h"
#include "../../../app/SerialOut.h"

#include "../../../app/PrintKv.h"
void AnalyzerApp::printSystemHealth(const AnalyzerReport& report) {
    const AudioSourceStats& sourceStats = _audioSource.stats();
    const AudioSignalStats& signalStats = _audioSignal.stats();
    const unsigned long sampleRateHz =
        _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : runtime::kDefaultAudioI2SSampleRateHz;
    const bool compactHealth = _sequenceTest.config.outputConfig.verbosity == 0U;
    const bool hasTrialWindow = _sequenceTest.active && _sequenceTest.trial.number > 0 &&
        timing::atOrAfter(_sequenceTest.trial.endMs, _sequenceTest.trial.startMs);
    const unsigned long windowStartMs = hasTrialWindow ? _sequenceTest.trial.startMs : activeRunStartMs();
    const unsigned long windowEndMs = hasTrialWindow ? _sequenceTest.trial.endMs : activeRunEndMs();
    const unsigned long windowElapsedMs = windowStartMs > 0 ? timing::elapsedOrZero(windowEndMs, windowStartMs) : 0UL;
    const unsigned long expectedSamples = static_cast<unsigned long>(
        (static_cast<unsigned long long>(windowElapsedMs) * static_cast<unsigned long long>(sampleRateHz)) / 1000ULL);
    const unsigned long processedSamples = hasTrialWindow
        ? _sequenceTest.trial.samplesProcessed
        : (_sequenceTest.active ? _sequenceTest.totals.samplesProcessed
                                : static_cast<unsigned long>(signalStats.samplesProcessed));
    const float processedRatio =
        expectedSamples > 0 ? static_cast<float>(processedSamples) / static_cast<float>(expectedSamples) : 0.0f;
    const unsigned long loopAvgUs =
        _loopHealth.count > 0 ? static_cast<unsigned long>(_loopHealth.sumUs / _loopHealth.count) : 0UL;
    const unsigned long updateLoopAvgUs = _sequenceTest.trial.updateLoopCount > 0
        ? static_cast<unsigned long>(_sequenceTest.trial.totalUpdateLoopUs / _sequenceTest.trial.updateLoopCount)
        : 0UL;

    serialOut().print("SYSTEM_HEALTH");
    printKv("loop_avg_us", loopAvgUs);
    printKv("update_loop_avg_us", updateLoopAvgUs);
    if (!compactHealth) {
        printKv("trial", report.context.trial);
        printKv("t_ms", report.context.timestampMs);
        printKv("uptime_ms", timebase::nowMs());
        printKv("boot_count", analyzerBootCount());
        printKv("reset_reason", currentResetReasonName());
        printKv("window_start_ms", windowStartMs);
        printKv("window_end_ms", windowEndMs);
        printKv("window_elapsed_ms", windowElapsedMs);
        printKv("free_heap", esp_get_free_heap_size());
        printKv("loop_stack_free_min_bytes", static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
        printKv("loop_stats_window_ms", windowElapsedMs);
        printKv("loop_stats_scope",
                _sequenceTest.active && _sequenceTest.trial.startMs > 0 ? "trial_window" : "run_window");
        printKv("loop_count", _loopHealth.count);
        printKv("loop_max_window_us", _loopHealth.maxUs);
        printKv("loop_over_5ms", _loopHealth.over5ms);
        printKv("loop_over_20ms", _loopHealth.over20ms);
        printKv("loop_max_since_boot_us", _loopMaxSinceBootUs);
        printKv("update_loop_count", _sequenceTest.trial.updateLoopCount);
        printKv("update_loop_max_us", _sequenceTest.trial.updateLoopMaxUs);
        printKv("sample_work_max_us", _sequenceTest.totals.maxSampleWorkUs);
        printKv("trial_finalize_max_us", _sequenceTest.totals.maxFinalizeTrialUs);
        printKv("detector_observe_count", _detection.diagnostics().observeFrameCount());
        printKv("detector_fresh_input_count", _detection.diagnostics().freshDetectorInputCount());
        printKv("detector_drain_count", _detection.diagnostics().detectorDrainCount());
        printKv("detector_report_refresh_count", _detection.diagnostics().detectorReportRefreshCount());
        printKv("detector_report_mismatch_count", _detection.diagnostics().detectorReportMismatchCount());
        printKv("no_fresh_frequency_skip_count", _detection.diagnostics().noFreshFrequencySkipCount());
        printKv("detector_occurrence_popped_count", _detection.diagnostics().detectorOccurrencePoppedCount());
        printKv("detector_valid_occurrence_popped_count",
                _detection.diagnostics().detectorValidOccurrencePoppedCount());
        printKv("verdict_produced_count", _detection.diagnostics().verdictProducedCount());
        printKv("verdict_event_pushed_count", _detection.diagnostics().verdictEventPushedCount());
        printKv("verdict_event_dropped_count", _detection.diagnostics().verdictEventDroppedCount());
    }
    serialOut().println();
    _loopHealth.reset();

    serialOut().print("AUDIO_IO_HEALTH");
    printKv("processed_ratio", processedRatio, 3);
    if (!compactHealth) {
        printKv("t_ms", report.context.timestampMs);
        printKv("i2s_reads", sourceStats.reads);
        printKv("i2s_bytes_read", sourceStats.readBytes);
        printKv("processed_samples", processedSamples);
        printKv("expected_samples", expectedSamples);
        serialOut().print(" frames_emitted=");
        serialOut().print(static_cast<unsigned long long>(signalStats.samplesProcessed));  // one frame per sample
        printKv("partial_frames", sourceStats.shortReads);
        serialOut().print(" duplicate_frames=0");
        serialOut().print(" gap_samples=0");
        serialOut().print(" overlap_samples=0");
        printKv("max_block_age_ms", _sequenceTest.active ? _sequenceTest.totals.maxBlockAgeMs : 0UL);
        printKv("max_processing_lag_ms", _sequenceTest.active ? _sequenceTest.totals.maxProcessingLagMs : 0UL);
        serialOut().println();
    } else {
        serialOut().println();
    }

    const auto& rawDiagnostics = _sequenceTest.trial.diagnostics;
    const float rawDcMean = rawDiagnostics.rawFrames > 0
        ? static_cast<float>(rawDiagnostics.rawSum) / static_cast<float>(rawDiagnostics.rawFrames)
        : 0.0f;
    const float rawMeanAbs = rawDiagnostics.rawFrames > 0
        ? static_cast<float>(rawDiagnostics.rawAbsSum) / static_cast<float>(rawDiagnostics.rawFrames)
        : 0.0f;
    const int32_t rawMinAbs = rawDiagnostics.rawMin < 0 ? -static_cast<int32_t>(rawDiagnostics.rawMin)
                                                        : static_cast<int32_t>(rawDiagnostics.rawMin);
    const int32_t rawMaxAbsValue = rawDiagnostics.rawMax < 0 ? -static_cast<int32_t>(rawDiagnostics.rawMax)
                                                             : static_cast<int32_t>(rawDiagnostics.rawMax);
    const uint16_t rawMaxAbs = static_cast<uint16_t>(rawMinAbs > rawMaxAbsValue ? rawMinAbs : rawMaxAbsValue);
    const float rawZeroRatio = rawDiagnostics.rawFrames > 0
        ? static_cast<float>(_sequenceTest.trial.diagnostics.audioZeroishFrames) /
            static_cast<float>(rawDiagnostics.rawFrames)
        : 0.0f;
    const char* rawHealth = rawHealthClassNameFromCounters(
        _sequenceTest.trial.diagnostics.audioRmsTooHighFrames, rawDiagnostics.rawFrames, rawMaxAbs, rawDcMean,
        rawMeanAbs, rawDiagnostics.rawMin, rawDiagnostics.rawMax, 0.0f, 0UL, 0UL,
        _sequenceTest.trial.diagnostics.audioFlatlineFrames, _sequenceTest.trial.diagnostics.audioZeroishFrames,
        _sequenceTest.trial.diagnostics.audioLargeJumpFrames, _sequenceTest.trial.diagnostics.audioRms,
        _sequenceTest.trial.diagnostics.audioRmsTooLowFrames, _sequenceTest.trial.diagnostics.audioRmsTooHighFrames);
    if (_sequenceTest.config.outputConfig.verbosity >= 1U) {
        serialOut().print("RAW_AUDIO_HEALTH");
        printKv("verdict", rawHealth != nullptr ? rawHealth : "unknown");
    }
    if (_sequenceTest.config.outputConfig.verbosity >= 2U) {
        printKv("t_ms", report.context.timestampMs);
        printKv("frames", rawDiagnostics.rawFrames);
        printKv("min_sample", rawDiagnostics.rawFrames > 0 ? rawDiagnostics.rawMin : 0);
        printKv("max_sample", rawDiagnostics.rawFrames > 0 ? rawDiagnostics.rawMax : 0);
        printKv("dc_mean", rawDcMean, 1);
        printKv("mean_abs", rawMeanAbs, 1);
        printKv("max_abs", rawMaxAbs);
        printKv("zero_ratio", rawZeroRatio, 3);
        printKv("clip_count", _sequenceTest.trial.diagnostics.audioRmsTooHighFrames);
        printKv("flatline", rawHealth != nullptr && strcmp(rawHealth, "flatline") == 0 ? 1 : 0);
        serialOut().println();
    } else if (_sequenceTest.config.outputConfig.verbosity >= 1U) {
        serialOut().println();
    }

    if (!shouldPrintHardwareDiagnostics()) {
        return;
    }

    serialOut().print("I2S_IO_HEALTH");
    printKv("reads", sourceStats.reads);
    printKv("bytes", sourceStats.readBytes);
    printKv("zero_reads", sourceStats.zeroReads);
    printKv("short_reads", sourceStats.shortReads);
    printKv("max_read_bytes", sourceStats.maxReadBytes);
    printKv("read_errors", sourceStats.readErrors);
    printKv("overflow", sourceStats.overflowCount);
    printKv("total_samples", static_cast<unsigned long long>(sourceStats.totalSamplesRead));
    serialOut().println();
}
