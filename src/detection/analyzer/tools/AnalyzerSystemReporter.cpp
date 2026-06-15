#include "AnalyzerSystemReporter.h"

#include "../../../modes/analyzer/AnalyzerModeApp.h"
#include "AnalyzerRawHealth.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

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
