#include "AnalyzerRuntimeReporter.h"

#include "../../../modes/analyzer/AnalyzerModeApp.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

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
    const unsigned long freqHistoryTargetRecords =
        _detection.featureHistory().sampleCount(detection::FeatureStreamId::FrequencyTarget);
    const unsigned long freqHistoryTargetBandRecords =
        _detection.featureHistory().sampleCount(detection::FeatureStreamId::FrequencyTargetBand);
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
    Serial.print(" history_target_records=");
    Serial.print(freqHistoryTargetRecords);
    Serial.print(" history_score_records=");
    Serial.print(freqHistoryScoreRecords);
    Serial.print(" history_target_band_records=");
    Serial.print(freqHistoryTargetBandRecords);
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
