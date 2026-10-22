#include "AnalyzerRuntimeReporter.h"

#include "../../../modes/analyzer/AnalyzerModeApp.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "../../../app/TimeBase.h"
#include "../../../app/TimingUtils.h"
#include "../../../app/SerialOut.h"

// History is recorded only for the streams the active inspection plan names,
// so report exactly those rather than a fixed pair: a stream the plan doesn't
// use has no buffer and would read as 0, which says nothing about the run.
void AnalyzerApp::printFeatureHistoryRecordCounts() const {
    const detection::FeatureHistory& history = _detection.featureHistory();
    serialOut().print(" history_records=");
    const size_t count = history.activeStreamCount();
    if (count == 0) {
        serialOut().print("none");
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        const detection::FeatureStreamId stream = history.activeStream(i);
        if (i > 0) {
            serialOut().print(',');
        }
        serialOut().print(detection::featureStreamName(stream));
        serialOut().print(':');
        serialOut().print(static_cast<unsigned long>(history.sampleCount(stream)));
    }
}

void AnalyzerApp::printDetectionParameters() const {
    //PARAM TUNING TEMPORARY
    const detection::DetectionProfile& activeProfile = effectiveSequenceProfile();

    const detection::ScalarTransientConfig& scalar = activeProfile.scalarTransient;
    serialOut().print("SEQ scalar:");
    serialOut().print(" observed_stream=");
    serialOut().print(scalarObservedStreamDisplayName(scalar.observedStream));
    serialOut().print(" onset_threshold=");
    serialOut().print(scalar.onsetDetectionThreshold, 1);
    serialOut().print(" release_threshold=");
    serialOut().print(scalar.onsetReleaseThreshold, 1);
    serialOut().print(" cooldown_ms=");
    serialOut().print(scalar.cooldownAfterOnsetMs);
    serialOut().print(" min_duration_ms=");
    serialOut().print(scalar.minTransientDurationMs);
    serialOut().print(" max_duration_ms=");
    serialOut().print(scalar.maxTransientDurationMs);
    serialOut().print(" min_peak_strength=");
    serialOut().print(scalar.minTransientPeakStrength, 1);
    serialOut().print(" release_debounce_ms=");
    serialOut().print(scalar.releaseDebounceMs);
    serialOut().print(" carrier_quality_required=");
    serialOut().print(scalar.requireCarrierQuality ? 1 : 0);
    serialOut().print(" min_strength_required=");
    serialOut().print(scalar.requireMinStrength ? 1 : 0);
    serialOut().print(" min_strength=");
    serialOut().print(scalar.minMatchedMeanStrength, 1);
    serialOut().print(" min_release_coverage_ms=");
    serialOut().print(scalar.minCoverageAboveReleaseMs);
    serialOut().print(" min_longest_island_ms=");
    serialOut().print(scalar.minLongestIslandMs);
    serialOut().print(" max_gap_ms=");
    serialOut().println(scalar.maxGapMs);

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

    serialOut().print("FREQBAND runtime:");
    serialOut().print(" freq.window_samples=");
    serialOut().print(windowSizeSamples);
    serialOut().print(" freq.window_ms=");
    serialOut().print(windowMs, 2);
    serialOut().print(" freq.update_every_samples=");
    serialOut().print(frequencyUpdateEverySamples);
    serialOut().print(" freq.update_period_ms=");
    serialOut().print(updateStepMs, 3);
    serialOut().print(" freq.target_hz=");
    serialOut().print(_freqBandStream.targetFrequencyHz());
    serialOut().print(" freq.produced_fresh_packet=");
    serialOut().print(_freqBandStream.producedFreshPacketOnLastObserve() ? 1 : 0);
    serialOut().print(" freq.packet_age_samples=");
    serialOut().print(ageSamples);
    serialOut().print(" freq.packet_age_ms=");
    serialOut().println(ageMs, 3);
}

void AnalyzerApp::printParamStatus() const {
    const detection::DetectionProfile& activeProfile = effectiveSequenceProfile();

    const detection::ScalarTransientConfig& scalar = activeProfile.scalarTransient;
    serialOut().print("PARAM scalar_observed_stream=");
    serialOut().print(scalarObservedStreamDisplayName(scalar.observedStream));
    serialOut().print(" scalar_onset_threshold=");
    serialOut().print(scalar.onsetDetectionThreshold, 1);
    serialOut().print(" scalar_release_threshold=");
    serialOut().print(scalar.onsetReleaseThreshold, 1);
    serialOut().print(" scalar_cooldown_ms=");
    serialOut().print(scalar.cooldownAfterOnsetMs);
    serialOut().print(" scalar_min_duration_ms=");
    serialOut().print(scalar.minTransientDurationMs);
    serialOut().print(" scalar_max_duration_ms=");
    serialOut().print(scalar.maxTransientDurationMs);
    serialOut().print(" scalar_min_peak_strength=");
    serialOut().print(scalar.minTransientPeakStrength, 1);
    serialOut().print(" scalar_release_debounce_ms=");
    serialOut().print(scalar.releaseDebounceMs);
    serialOut().print(" scalar_require_carrier_quality=");
    serialOut().print(scalar.requireCarrierQuality ? 1 : 0);
    serialOut().print(" scalar_require_min_strength=");
    serialOut().print(scalar.requireMinStrength ? 1 : 0);
    serialOut().print(" scalar_min_strength=");
    serialOut().print(scalar.minMatchedMeanStrength, 1);
    serialOut().print(" scalar_min_release_coverage_ms=");
    serialOut().print(scalar.minCoverageAboveReleaseMs);
    serialOut().print(" scalar_min_longest_island_ms=");
    serialOut().print(scalar.minLongestIslandMs);
    serialOut().print(" scalar_max_gap_ms=");
    serialOut().print(scalar.maxGapMs);

    serialOut().print(" freqScore=");
    serialOut().print(_analyzerTuning.frequencyMatch.attackScoreMin, 1);
    serialOut().print(" freqContrast=");
    serialOut().print(_analyzerTuning.frequencyMatch.attackContrastMin, 1);
    serialOut().print(" freqReleaseScore=");
    serialOut().print(_analyzerTuning.frequencyMatch.releaseScoreMin, 1);
    serialOut().print(" freqReleaseContrast=");
    serialOut().println(_analyzerTuning.frequencyMatch.releaseContrastMin, 1);
}

void AnalyzerApp::printAudioSourceSummary() const {
    const AudioSourceStats& stats = _audioSource.stats();
    serialOut().println("AUDIO summary:");
    serialOut().print("reads=");
    serialOut().print(stats.reads);
    serialOut().print(" readBytes=");
    serialOut().print(stats.readBytes);
    serialOut().print(" zeroReads=");
    serialOut().print(stats.zeroReads);
    serialOut().print(" shortReads=");
    serialOut().print(stats.shortReads);
    serialOut().print(" maxReadBytes=");
    serialOut().print(stats.maxReadBytes);
    serialOut().print(" noSampleLoops=");
    serialOut().print(stats.noSampleLoops);
    serialOut().print(" readErrors=");
    serialOut().print(stats.readErrors);
    serialOut().print(" overflow=");
    serialOut().print(stats.overflowCount);
    serialOut().print(" totalSamples=");
    serialOut().println(static_cast<unsigned long long>(stats.totalSamplesRead));
}

void AnalyzerApp::printAudioRunSummary() const {
    if (!_sequenceTest.active && _sequenceTest.completedTrials == 0) {
        return;
    }

    const unsigned long now = timebase::nowMs();
    const unsigned long sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const unsigned long runElapsedMs = _sequenceTest.startedAtMs > 0
        ? timing::elapsedOrZero(now, _sequenceTest.startedAtMs)
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

    serialOut().println("AUDIO run:");
    serialOut().print("run_elapsed_ms=");
    serialOut().print(runElapsedMs);
    serialOut().print(" expected_samples=");
    serialOut().print(expectedSamples);
    serialOut().print(" processed_samples=");
    serialOut().print(processedSamples);
    serialOut().print(" processed_ratio=");
    serialOut().print(processedRatio, 3);
    serialOut().print(" max_available_bytes=");
    serialOut().print(_sequenceTest.maxAvailableBytes);
    serialOut().print(" avg_available_bytes=");
    serialOut().print(avgAvailableBytes);
    serialOut().print(" max_block_age_ms=");
    serialOut().print(_sequenceTest.maxBlockAgeMs);
    serialOut().print(" max_update_loop_us=");
    serialOut().print(_sequenceTest.maxUpdateLoopUs);
    serialOut().print(" max_processing_lag_ms=");
    serialOut().println(_sequenceTest.maxProcessingLagMs);

    serialOut().print("FREQBAND config:");
    serialOut().print(" freqband=");
    serialOut().print(_seqOutputConfig.frequencyBandEnabled ? "on" : "off");
    serialOut().print(" updateEverySamples=");
    serialOut().println(_seqOutputConfig.frequencyUpdateEverySamples);

    const unsigned long freqAgeSamples = _freqBandStream.lastPacketAgeSamples();
    const unsigned long freqComputedAtSample = _freqBandStream.sampleCount() >= freqAgeSamples
        ? _freqBandStream.sampleCount() - freqAgeSamples
        : 0UL;

    serialOut().print("FREQBAND profile:");
#if FREQBAND_ENABLE_PROFILING
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

    serialOut().print(" observe_calls=");
    serialOut().print(freqObserveCalls);
    serialOut().print(" avg_observe_us=");
    serialOut().print(avgObserveUs, 2);
    serialOut().print(" compute_calls=");
    serialOut().print(freqComputeCalls);
    serialOut().print(" avg_compute_us=");
    serialOut().print(avgComputeUs, 2);
    serialOut().print(" avg_energy_us=");
    serialOut().print(avgEnergyUs, 2);
    serialOut().print(" avg_goertzel_us=");
    serialOut().println(avgGoertzelUs, 2);
#else
    serialOut().println(" profiling=disabled");
#endif

    serialOut().print("FREQBAND freshness:");
#if FREQBAND_ENABLE_PROFILING
    const unsigned long freqFreshObserveCalls = _freqBandStream.profileComputeCalls();
    const unsigned long freqHeldObserveCalls = _freqBandStream.profileObserveCalls() > _freqBandStream.profileComputeCalls()
        ? _freqBandStream.profileObserveCalls() - _freqBandStream.profileComputeCalls()
        : 0UL;
    serialOut().print(" fresh_frames=");
    serialOut().print(freqFreshObserveCalls);
    serialOut().print(" held_frames=");
    serialOut().print(freqHeldObserveCalls);
    serialOut().print(" age_samples=");
    serialOut().print(freqAgeSamples);
    serialOut().print(" computed_at_sample=");
    serialOut().print(freqComputedAtSample);
    printFeatureHistoryRecordCounts();
    serialOut().println();
#else
    serialOut().print(" profiling=disabled");
    serialOut().print(" age_samples=");
    serialOut().print(freqAgeSamples);
    serialOut().print(" computed_at_sample=");
    serialOut().print(freqComputedAtSample);
    printFeatureHistoryRecordCounts();
    serialOut().println();
#endif
}

void AnalyzerApp::printOccurrenceSummary() const {
    const AudioSignalStats& stats = _audioSignal.stats();
    serialOut().println("OCCURRENCE summary:");
    serialOut().print("blocks=");
    serialOut().print(stats.blocksProcessed);
    serialOut().print(" samples=");
    serialOut().print(static_cast<unsigned long long>(stats.samplesProcessed));
    serialOut().print(" lastBlockStart=");
    serialOut().print(static_cast<unsigned long long>(_audioSignal.lastBlockStartSample()));
    serialOut().print(" lastBlockCount=");
    serialOut().print(_audioSignal.lastBlockSampleCount());
    serialOut().print(" lastBlockMicros=");
    serialOut().println(static_cast<unsigned long>(_audioSignal.lastBlockApproxStartUs64()));
}
