#include "AnalyzerRuntimeReporter.h"
#include "../../../app/FreqStatusFields.h"

#include "../AnalyzerModeApp.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "../../../app/TimeBase.h"
#include "../../../app/TimingUtils.h"
#include "../../../app/SerialOut.h"

#include "../../../app/PrintKv.h"
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
    printKv("observed_stream", scalarObservedStreamDisplayName(scalar.observedStream));
    printKv("onset_threshold", scalar.onsetDetectionThreshold, 1);
    printKv("release_threshold", scalar.onsetReleaseThreshold, 1);
    printKv("cooldown_ms", scalar.cooldownAfterOnsetMs);
    printKv("min_duration_ms", scalar.minTransientDurationMs);
    printKv("max_duration_ms", scalar.maxTransientDurationMs);
    printKv("min_peak_strength", scalar.minTransientPeakStrength, 1);
    printKv("release_debounce_ms", scalar.releaseDebounceMs);
    printKv("carrier_quality_required", scalar.requireCarrierQuality ? 1 : 0);
    printKv("min_strength_required", scalar.requireMinStrength ? 1 : 0);
    printKv("min_strength", scalar.minMatchedMeanStrength, 1);
    printKv("min_release_coverage_ms", scalar.minCoverageAboveReleaseMs);
    printKv("min_longest_island_ms", scalar.minLongestIslandMs);
    printKv("max_gap_ms", scalar.maxGapMs);
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
    printKv("readBytes", stats.readBytes);
    printKv("zeroReads", stats.zeroReads);
    printKv("shortReads", stats.shortReads);
    printKv("maxReadBytes", stats.maxReadBytes);
    printKv("noSampleLoops", stats.noSampleLoops);
    printKv("readErrors", stats.readErrors);
    printKv("overflow", stats.overflowCount);
    printKv("totalSamples", static_cast<unsigned long long>(stats.totalSamplesRead));
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
    printKv("expected_samples", expectedSamples);
    printKv("processed_samples", processedSamples);
    printKv("processed_ratio", processedRatio, 3);
    printKv("max_available_bytes", _sequenceTest.totals.maxAvailableBytes);
    printKv("avg_available_bytes", avgAvailableBytes);
    printKv("max_block_age_ms", _sequenceTest.totals.maxBlockAgeMs);
    printKv("max_update_loop_us", _sequenceTest.totals.maxUpdateLoopUs);
    printKv("max_processing_lag_ms", _sequenceTest.totals.maxProcessingLagMs);
    serialOut().println();

    serialOut().print("FREQBAND config:");
    printKv("updateEverySamples", _seqOutputConfig.frequencyUpdateEverySamples);
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

    printKv("observe_calls", freqObserveCalls);
    printKv("avg_observe_us", avgObserveUs, 2);
    printKv("compute_calls", freqComputeCalls);
    printKv("avg_compute_us", avgComputeUs, 2);
    printKv("avg_energy_us", avgEnergyUs, 2);
    printKv("avg_goertzel_us", avgGoertzelUs, 2);
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
    printKv("fresh_frames", freqFreshObserveCalls);
    printKv("held_frames", freqHeldObserveCalls);
    printKv("age_samples", freqAgeSamples);
    printKv("computed_at_sample", freqComputedAtSample);
    printFeatureHistoryRecordCounts();
    serialOut().println();
#else
    serialOut().print(" profiling=disabled");
    printKv("age_samples", freqAgeSamples);
    printKv("computed_at_sample", freqComputedAtSample);
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
    printKv("lastBlockStart", static_cast<unsigned long long>(latest.sampleIndex));
    printKv("lastBlockCount", latest.valid ? 1 : 0);
    printKv("lastBlockMicros", static_cast<unsigned long>(latest.timeUs64));
    serialOut().println();
}
