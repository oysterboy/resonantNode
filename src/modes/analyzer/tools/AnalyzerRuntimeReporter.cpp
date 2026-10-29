#include "AnalyzerRuntimeReporter.h"
#include "../../../app/FreqStatusFields.h"

#include "../AnalyzerModeApp.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "../../../app/TimeBase.h"
#include "../../../app/TimingUtils.h"
#include "../../../app/SerialOut.h"

#include "../../../app/PrintKeyValue.h"
// History is recorded only for the streams the active inspection plan names,
// so report exactly those rather than a fixed pair: a stream the plan doesn't
// use has no buffer and would read as 0, which says nothing about the run.
void AnalyzerApp::printFeatureHistoryRecordCounts() const {
    const detection::FeatureHistory& history = _detection.diagnostics().featureHistory();
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
    const detection::DetectionProfile& activeProfile = effectiveSequenceProfile();

    const detection::ScalarTransientConfig& scalar = activeProfile.scalarTransient;
    serialOut().print("SEQ scalar:");
    printKeyValue("observed_stream", scalarObservedStreamDisplayName(scalar.observedStream));
    printKeyValue("onset_threshold", scalar.onsetDetectionThreshold, 1);
    printKeyValue("release_threshold", scalar.onsetReleaseThreshold, 1);
    printKeyValue("cooldown_ms", scalar.cooldownAfterOnsetMs);
    printKeyValue("min_duration_ms", scalar.minTransientDurationMs);
    printKeyValue("max_duration_ms", scalar.maxTransientDurationMs);
    printKeyValue("min_peak_strength", scalar.minTransientPeakStrength, 1);
    printKeyValue("release_debounce_ms", scalar.releaseDebounceMs);
    printKeyValue("carrier_quality_required", scalar.requireCarrierQuality ? 1 : 0);
    printKeyValue("min_strength_required", scalar.requireMinStrength ? 1 : 0);
    printKeyValue("min_strength", scalar.minMatchedMeanStrength, 1);
    printKeyValue("min_release_coverage_ms", scalar.minCoverageAboveReleaseMs);
    printKeyValue("min_longest_island_ms", scalar.minLongestIslandMs);
    printKeyValue("max_gap_ms", scalar.maxGapMs);
    serialOut().println();

    const unsigned long sampleRateHz =
        _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : runtime::kDefaultAudioI2SSampleRateHz;
    serialOut().print("FREQBAND runtime:");
    printFreqStatusFields(serialOut(), _freqBandStream, sampleRateHz);
    serialOut().println();
}

void AnalyzerApp::printAudioSourceSummary() const {
    const AudioSourceStats& stats = _audioSource.stats();
    serialOut().println("AUDIO summary:");
    serialOut().print("reads=");
    serialOut().print(stats.reads);
    printKeyValue("readBytes", stats.readBytes);
    printKeyValue("zeroReads", stats.zeroReads);
    printKeyValue("shortReads", stats.shortReads);
    printKeyValue("maxReadBytes", stats.maxReadBytes);
    printKeyValue("noSampleLoops", stats.noSampleLoops);
    printKeyValue("readErrors", stats.readErrors);
    printKeyValue("overflow", stats.overflowCount);
    printKeyValue("totalSamples", static_cast<unsigned long long>(stats.totalSamplesRead));
    serialOut().println();
}

void AnalyzerApp::printAudioRunSummary() const {
    if (!_sequenceTest.active && _sequenceTest.totals.completedTrials == 0) {
        return;
    }

    const unsigned long now = timebase::nowMs();
    const unsigned long sampleRateHz =
        _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : runtime::kDefaultAudioI2SSampleRateHz;
    const unsigned long runElapsedMs =
        _sequenceTest.startedAtMs > 0 ? timing::elapsedOrZero(now, _sequenceTest.startedAtMs) : 0UL;
    const unsigned long expectedSamples = static_cast<unsigned long>(
        (static_cast<unsigned long long>(runElapsedMs) * static_cast<unsigned long long>(sampleRateHz)) / 1000ULL);
    const unsigned long processedSamples = _sequenceTest.totals.samplesProcessed;
    const float processedRatio =
        expectedSamples > 0 ? static_cast<float>(processedSamples) / static_cast<float>(expectedSamples) : 0.0f;
    const unsigned long avgAvailableBytes = _sequenceTest.totals.availableBytesSamples > 0
        ? static_cast<unsigned long>(_sequenceTest.totals.availableBytesSum /
                                     static_cast<uint64_t>(_sequenceTest.totals.availableBytesSamples))
        : 0UL;

    serialOut().println("AUDIO run:");
    serialOut().print("run_elapsed_ms=");
    serialOut().print(runElapsedMs);
    printKeyValue("expected_samples", expectedSamples);
    printKeyValue("processed_samples", processedSamples);
    printKeyValue("processed_ratio", processedRatio, 3);
    printKeyValue("max_available_bytes", _sequenceTest.totals.maxAvailableBytes);
    printKeyValue("avg_available_bytes", avgAvailableBytes);
    printKeyValue("max_block_age_ms", _sequenceTest.totals.maxBlockAgeMs);
    printKeyValue("max_update_loop_us", _sequenceTest.totals.maxUpdateLoopUs);
    printKeyValue("max_processing_lag_ms", _sequenceTest.totals.maxProcessingLagMs);
    serialOut().println();

    serialOut().print("FREQBAND config:");
    printKeyValue("updateEverySamples", _seqOutputConfig.frequencyUpdateEverySamples);
    serialOut().println();

    const unsigned long freqAgeSamples = _freqBandStream.lastPacketAgeSamples();
    const unsigned long freqComputedAtSample =
        _freqBandStream.sampleCount() >= freqAgeSamples ? _freqBandStream.sampleCount() - freqAgeSamples : 0UL;

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

    printKeyValue("observe_calls", freqObserveCalls);
    printKeyValue("avg_observe_us", avgObserveUs, 2);
    printKeyValue("compute_calls", freqComputeCalls);
    printKeyValue("avg_compute_us", avgComputeUs, 2);
    printKeyValue("avg_energy_us", avgEnergyUs, 2);
    printKeyValue("avg_goertzel_us", avgGoertzelUs, 2);
    serialOut().println();
#else
    serialOut().println(" profiling=disabled");
#endif

    serialOut().print("FREQBAND freshness:");
#if FREQBAND_ENABLE_PROFILING
    const unsigned long freqFreshObserveCalls = _freqBandStream.profileComputeCalls();
    const unsigned long freqHeldObserveCalls =
        _freqBandStream.profileObserveCalls() > _freqBandStream.profileComputeCalls()
        ? _freqBandStream.profileObserveCalls() - _freqBandStream.profileComputeCalls()
        : 0UL;
    printKeyValue("fresh_frames", freqFreshObserveCalls);
    printKeyValue("held_frames", freqHeldObserveCalls);
    printKeyValue("age_samples", freqAgeSamples);
    printKeyValue("computed_at_sample", freqComputedAtSample);
    printFeatureHistoryRecordCounts();
    serialOut().println();
#else
    serialOut().print(" profiling=disabled");
    printKeyValue("age_samples", freqAgeSamples);
    printKeyValue("computed_at_sample", freqComputedAtSample);
    printFeatureHistoryRecordCounts();
    serialOut().println();
#endif
}

void AnalyzerApp::printOccurrenceSummary() const {
    const AudioSignalStats& stats = _audioSignal.stats();
    serialOut().println("OCCURRENCE summary:");
    serialOut().print("samples=");
    serialOut().print(static_cast<unsigned long long>(stats.samplesProcessed));
    // The signal layer runs per sample (one-sample "blocks"): the last block is
    // the latest frame. Labels kept for the log readers.
    const AudioSamplePacket& latest = _audioSignal.latestFrame();
    printKeyValue("lastBlockStart", static_cast<unsigned long long>(latest.sampleIndex));
    printKeyValue("lastBlockCount", latest.valid ? 1 : 0);
    printKeyValue("lastBlockMicros", static_cast<unsigned long>(latest.timeUs64));
    serialOut().println();
}
