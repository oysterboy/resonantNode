#include "AnalyzerApp.h"

#include <Arduino.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr unsigned long kRawCaptureFlushSamples = 256;
constexpr unsigned long kRawCaptureTimeoutSlackMs = 2000;

int16_t rawCaptureSampleToInt16(int sample) {
    const int32_t shifted = static_cast<int32_t>(sample) >> 16;
    if (shifted > 32767) {
        return 32767;
    }
    if (shifted < -32768) {
        return -32768;
    }
    return static_cast<int16_t>(shifted);
}

int16_t clampCenteredSampleToInt16(int sample) {
    if (sample > 32767) {
        return 32767;
    }
    if (sample < -32768) {
        return -32768;
    }
    return static_cast<int16_t>(sample);
}

unsigned long rawCaptureChunkSize(unsigned long sampleRateHz, unsigned long decim) {
    const unsigned long baseChunk = sampleRateHz / 20UL;
    const unsigned long decimatedChunk = decim > 0 ? baseChunk / decim : baseChunk;
    return decimatedChunk > 0 ? decimatedChunk : 1UL;
}

} // namespace

void AnalyzerApp::runRawBandTrigger(unsigned long toneHz,
                                    unsigned long durationMs,
                                    unsigned long postMs,
                                    unsigned long preMs,
                                    unsigned long decim) {
    if (_valMode) {
        return;
    }

    stopSequenceTest();
    stopCaptureSession();
    resetAudioSignalState();
    _audioSource.resetStats();
    _audioSignal.resetSignalState();

    if (toneHz == 0) {
        toneHz = 1;
    }
    if (durationMs == 0) {
        durationMs = 1;
    }
    if (postMs == 0) {
        postMs = 1;
    }
    if (decim == 0) {
        decim = 1;
    }

    const uint32_t sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const unsigned long maxSamples = 16000UL;
    unsigned long preWantedSamples = static_cast<unsigned long>((static_cast<uint64_t>(preMs) * static_cast<uint64_t>(sampleRateHz)) / 1000ULL);
    unsigned long postWantedSamples = static_cast<unsigned long>((static_cast<uint64_t>(postMs) * static_cast<uint64_t>(sampleRateHz)) / 1000ULL);
    if (preWantedSamples > maxSamples) {
        preWantedSamples = maxSamples;
    }
    if (postWantedSamples > maxSamples) {
        postWantedSamples = maxSamples;
    }
    if (preWantedSamples + postWantedSamples > maxSamples) {
        if (preWantedSamples >= maxSamples) {
            preWantedSamples = maxSamples;
            postWantedSamples = 0;
        } else {
            postWantedSamples = maxSamples - preWantedSamples;
        }
    }
    if (preWantedSamples == 0 && postWantedSamples == 0) {
        postWantedSamples = 1;
    }

    FreqBandStream band;
    band.resetState();
    band.setTargetFrequencyHz(_freqBandStream.targetFrequencyHz());
    band.setSampleRateHz(sampleRateHz);
    band.setWindowSizeSamples(_freqBandStream.windowSizeSamples());
    band.setFrequencyUpdateEverySamples(decim);

    int16_t* rawBuffer = static_cast<int16_t*>(malloc(static_cast<size_t>(maxSamples) * sizeof(int16_t)));
    if (rawBuffer == nullptr) {
        Serial.println("RAWBAND_ERR memory=band_buffer_alloc_failed");
        return;
    }
    unsigned long flushedSamples = 0;
    int discardedSample = 0;
    uint32_t discardedSampleTimeUs = 0;
    while (flushedSamples < kRawCaptureFlushSamples && _audioSource.readRawSample(discardedSample, discardedSampleTimeUs)) {
        ++flushedSamples;
    }

    const unsigned long captureId = ++_rawCaptureSequenceId;
    const unsigned long triggerMs = millis() + preMs;

    unsigned long capturedSamples = 0;
    unsigned long freshSamples = 0;
    unsigned long heldSamples = 0;
    float maxScore = 0.0f;
    float maxContrast = 0.0f;
    unsigned long sampleIndex = 0;

    auto captureAndStoreSample = [&]() {
        int rawSample = 0;
        uint32_t sampleTimeUs = 0;
        if (!_audioSource.readRawSample(rawSample, sampleTimeUs)) {
            return false;
        }

        AudioSamplePacket audioSamplePacket = {};
        if (!_audioSignal.update(rawSample, sampleTimeUs, audioSamplePacket)) {
            return false;
        }

        const int centeredSample = audioSamplePacket.centeredAudioValue;
        band.observeCenteredSample(centeredSample, audioSamplePacket.timeMs);
        if (band.producedFreshPacketOnLastObserve()) {
            ++freshSamples;
        } else {
            ++heldSamples;
        }
        if (band.lastTargetBandScoreValue() > maxScore) {
            maxScore = band.lastTargetBandScoreValue();
        }
        if (band.lastTargetBandContrastValue() > maxContrast) {
            maxContrast = band.lastTargetBandContrastValue();
        }

        rawBuffer[capturedSamples < maxSamples ? capturedSamples : (maxSamples - 1UL)] = clampCenteredSampleToInt16(centeredSample);

        ++sampleIndex;
        ++capturedSamples;
        return true;
    };

    const unsigned long preDeadlineMs = millis() + preMs + kRawCaptureTimeoutSlackMs;
    while (sampleIndex < preWantedSamples && millis() <= preDeadlineMs) {
        if (!captureAndStoreSample()) {
            delay(1);
        }
    }

    char emitterCommand[96];
    snprintf(emitterCommand, sizeof(emitterCommand), "CHIRP freq=%lu dur=%lu", toneHz, durationMs);
    sendEmitterCommand(emitterCommand);
    Serial2.flush();

    const unsigned long postDeadlineMs = millis() + postMs + kRawCaptureTimeoutSlackMs;
    while ((capturedSamples - preWantedSamples) < postWantedSamples && millis() <= postDeadlineMs) {
        if (!captureAndStoreSample()) {
            delay(1);
        }
    }

    Serial.print("RAWBAND_BEGIN id=");
    Serial.print(captureId);
    Serial.print(" sr=");
    Serial.print(sampleRateHz);
    Serial.print(" trigger_ms=");
    Serial.print(triggerMs);
    Serial.print(" f=");
    Serial.print(toneHz);
    Serial.print(" dur=");
    Serial.print(durationMs);
    Serial.print(" pre_ms=");
    Serial.print(preMs);
    Serial.print(" post_ms=");
    Serial.print(postMs);
    Serial.print(" decim=");
    Serial.print(decim);
    Serial.print(" metric=contrast");
    Serial.println();

    FreqBandStream dumpBand;
    dumpBand.resetState();
    dumpBand.setTargetFrequencyHz(_freqBandStream.targetFrequencyHz());
    dumpBand.setSampleRateHz(sampleRateHz);
    dumpBand.setWindowSizeSamples(_freqBandStream.windowSizeSamples());
    dumpBand.setFrequencyUpdateEverySamples(decim);

    for (unsigned long i = 0; i < capturedSamples; ++i) {
        dumpBand.observeCenteredSample(static_cast<int>(rawBuffer[i]));
        const char* phase = i < preWantedSamples ? "pre" : "post";
        Serial.print("RAWBAND_SAMPLE phase=");
        Serial.print(phase);
        Serial.print(" i=");
        Serial.print(i);
        Serial.print(" centered=");
        Serial.print(rawBuffer[i]);
        Serial.print(" score=");
        Serial.print(dumpBand.lastTargetBandScoreValue(), 2);
        Serial.print(" contrast=");
        Serial.print(dumpBand.lastTargetBandContrastValue(), 2);
        Serial.print(" target_power=");
        Serial.print(dumpBand.lastTargetBandPowerValue(), 1);
        Serial.print(" neighbor_power=");
        Serial.print(dumpBand.lastNeighborBandPowerValue(), 1);
        Serial.print(" total_energy=");
        Serial.print(dumpBand.lastTotalEnergyValue(), 1);
        Serial.print(" updated=");
        Serial.print(dumpBand.producedFreshPacketOnLastObserve() ? 1 : 0);
        Serial.print(" age_samples=");
        Serial.println(dumpBand.lastPacketAgeSamples());
    }

    Serial.print("RAWBAND_SUMMARY id=");
    Serial.print(captureId);
    Serial.print(" captured=");
    Serial.print(capturedSamples);
    Serial.print(" fresh=");
    Serial.print(freshSamples);
    Serial.print(" held=");
    Serial.print(heldSamples);
    Serial.print(" max_score=");
    Serial.print(maxScore, 2);
    Serial.print(" max_contrast=");
    Serial.print(maxContrast, 2);
    Serial.println();

    free(rawBuffer);
}

void AnalyzerApp::runRawTrigger(unsigned long toneHz,
                                unsigned long durationMs,
                                unsigned long postMs,
                                unsigned long preMs,
                                unsigned long decim,
                                bool dumpChunks,
                                bool dumpBinary) {
    if (_valMode) {
        return;
    }

    stopSequenceTest();
    stopCaptureSession();
    resetAudioSignalState();
    _audioSource.resetStats();

    if (toneHz == 0) {
        toneHz = 1;
    }
    if (durationMs == 0) {
        durationMs = 1;
    }
    if (postMs == 0) {
        postMs = 1;
    }
    if (decim == 0) {
        decim = 1;
    }
    if (dumpChunks) {
        Serial.println("RAW_INFO dump=chunks");
    }

    const uint32_t sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const unsigned long maxSamples = 16000UL;
    unsigned long preWantedSamples = static_cast<unsigned long>((static_cast<uint64_t>(preMs) * static_cast<uint64_t>(sampleRateHz)) / 1000ULL);
    unsigned long postWantedSamples = static_cast<unsigned long>((static_cast<uint64_t>(postMs) * static_cast<uint64_t>(sampleRateHz)) / 1000ULL);
    if (preWantedSamples > maxSamples) {
        preWantedSamples = maxSamples;
    }
    if (postWantedSamples > maxSamples) {
        postWantedSamples = maxSamples;
    }
    if (preWantedSamples + postWantedSamples > maxSamples) {
        if (preWantedSamples >= maxSamples) {
            preWantedSamples = maxSamples;
            postWantedSamples = 0;
        } else {
            postWantedSamples = maxSamples - preWantedSamples;
        }
    }
    if (preWantedSamples == 0 && postWantedSamples == 0) {
        postWantedSamples = 1;
    }
    const unsigned long captureSamples = preWantedSamples + postWantedSamples;
    int16_t* preRingBuffer = nullptr;
    if (preWantedSamples > 0) {
        preRingBuffer = static_cast<int16_t*>(malloc(static_cast<size_t>(preWantedSamples) * sizeof(int16_t)));
        if (preRingBuffer == nullptr) {
            Serial.println("RAW_ERR memory=pre_ring_alloc_failed");
            return;
        }
    }

    static int16_t rawBuffer[16000];
    unsigned long flushedSamples = 0;
    int discardedSample = 0;
    uint32_t discardedSampleTimeUs = 0;
    while (flushedSamples < kRawCaptureFlushSamples && _audioSource.readRawSample(discardedSample, discardedSampleTimeUs)) {
        ++flushedSamples;
    }

    const unsigned long captureId = ++_rawCaptureSequenceId;
    unsigned long preCaptured = 0;
    unsigned long preWriteIndex = 0;
    const unsigned long preDeadlineMs = millis() + preMs + kRawCaptureTimeoutSlackMs;
    while (preCaptured < preWantedSamples && millis() <= preDeadlineMs) {
        int rawSample = 0;
        uint32_t sampleTimeUs = 0;
        if (_audioSource.readRawSample(rawSample, sampleTimeUs)) {
            preRingBuffer[preWriteIndex] = rawCaptureSampleToInt16(rawSample);
            preWriteIndex = (preWriteIndex + 1UL) % (preWantedSamples > 0 ? preWantedSamples : 1UL);
            ++preCaptured;
        } else {
            delay(1);
        }
    }
    if (preCaptured > 0) {
        const unsigned long preStartIndex = preCaptured == preWantedSamples ? preWriteIndex : 0UL;
        for (unsigned long i = 0; i < preCaptured; ++i) {
            const unsigned long ringIndex = (preStartIndex + i) % (preCaptured > 0 ? preCaptured : 1UL);
            rawBuffer[i] = preRingBuffer[ringIndex];
        }
    }

    const unsigned long triggerMs = millis();
    char emitterCommand[96];
    snprintf(emitterCommand, sizeof(emitterCommand), "CHIRP freq=%lu dur=%lu", toneHz, durationMs);
    sendEmitterCommand(emitterCommand);
    Serial2.flush();

    unsigned long postCaptured = 0;
    const unsigned long postDeadlineMs = triggerMs + postMs + kRawCaptureTimeoutSlackMs;
    while (postCaptured < postWantedSamples && millis() <= postDeadlineMs) {
        int rawSample = 0;
        uint32_t sampleTimeUs = 0;
        if (_audioSource.readRawSample(rawSample, sampleTimeUs)) {
            rawBuffer[preCaptured + postCaptured] = rawCaptureSampleToInt16(rawSample);
            ++postCaptured;
        } else {
            delay(1);
        }
    }

    const unsigned long capturedSamples = preCaptured + postCaptured;
    const unsigned long droppedSamples = captureSamples > capturedSamples ? (captureSamples - capturedSamples) : 0;

    float env = 0.0f;
    float maxEnv = 0.0f;
    int maxRaw = 0;
    int maxAbs = 0;
    for (unsigned long i = 0; i < capturedSamples; ++i) {
        const int rawSample = static_cast<int>(rawBuffer[i]);
        const int absSample = rawSample < 0 ? -rawSample : rawSample;
        env = env * 0.95f + static_cast<float>(absSample) * 0.05f;
        if (absSample > maxRaw) {
            maxRaw = absSample;
        }
        if (absSample > maxAbs) {
            maxAbs = absSample;
        }
        if (env > maxEnv) {
            maxEnv = env;
        }
    }

    Serial.print("RAW_BEGIN id=");
    Serial.print(captureId);
    Serial.print(" sr=");
    Serial.print(sampleRateHz);
    Serial.print(" trigger_ms=");
    Serial.print(triggerMs);
    Serial.print(" f=");
    Serial.print(toneHz);
    Serial.print(" dur=");
    Serial.print(durationMs);
    Serial.print(" pre_ms=");
    Serial.print(preMs);
    Serial.print(" post_ms=");
    Serial.print(postMs);
    Serial.print(" decim=");
    Serial.print(decim);
    Serial.print(" pre_samples=");
    Serial.print(preCaptured);
    Serial.print(" post_samples=");
    Serial.print(postCaptured);
    if (dumpBinary) {
        Serial.print(" fields=raw16");
        Serial.print(" dump=bin");
        Serial.print(" samples=");
        Serial.print(capturedSamples);
        Serial.print(" bytes=");
        Serial.print(capturedSamples * sizeof(int16_t));
    } else if (dumpChunks) {
        Serial.print(" fields=min,max,rms,mean_abs");
        Serial.print(" dump=chunks");
        Serial.print(" chunk_samples=");
        Serial.print(rawCaptureChunkSize(sampleRateHz, decim));
    } else {
        Serial.print(" fields=i,raw,abs,env");
        Serial.print(" dump=full");
    }
    Serial.println();

    if (dumpBinary) {
        Serial.write(reinterpret_cast<const uint8_t*>(rawBuffer), capturedSamples * sizeof(int16_t));
        Serial.println();
    } else if (dumpChunks) {
        const unsigned long emittedSamples = (capturedSamples + decim - 1UL) / decim;
        const unsigned long chunkSamples = rawCaptureChunkSize(sampleRateHz, decim);
        for (unsigned long emittedStart = 0; emittedStart < emittedSamples; emittedStart += chunkSamples) {
            const unsigned long emittedEnd = emittedStart + chunkSamples < emittedSamples ? emittedStart + chunkSamples : emittedSamples;
            long chunkMin = 0;
            long chunkMax = 0;
            uint64_t sumAbs = 0;
            uint64_t sumSquares = 0;
            bool first = true;
            for (unsigned long emittedIndex = emittedStart; emittedIndex < emittedEnd; ++emittedIndex) {
                const unsigned long rawIndex = emittedIndex * decim;
                if (rawIndex >= capturedSamples) {
                    break;
                }
                const int sample = static_cast<int>(rawBuffer[rawIndex]);
                const long absSample = sample < 0 ? -static_cast<long>(sample) : static_cast<long>(sample);
                if (first) {
                    chunkMin = sample;
                    chunkMax = sample;
                    first = false;
                } else {
                    if (sample < chunkMin) {
                        chunkMin = sample;
                    }
                    if (sample > chunkMax) {
                        chunkMax = sample;
                    }
                }
                const uint64_t abs64 = static_cast<uint64_t>(absSample);
                sumAbs += abs64;
                sumSquares += abs64 * abs64;
            }
            const unsigned long chunkCount = emittedEnd > emittedStart ? emittedEnd - emittedStart : 0UL;
            if (chunkCount == 0) {
                continue;
            }
            const double meanAbs = static_cast<double>(sumAbs) / static_cast<double>(chunkCount);
            const double rms = sqrt(static_cast<double>(sumSquares) / static_cast<double>(chunkCount));
            const long i0 = static_cast<long>(emittedStart * decim) - static_cast<long>(preCaptured);
            const unsigned long lastRawIndex = (emittedEnd - 1UL) * decim;
            const long i1 = static_cast<long>(lastRawIndex < capturedSamples ? lastRawIndex : (capturedSamples - 1UL)) - static_cast<long>(preCaptured);
            Serial.print("RAW_CHUNK i0=");
            Serial.print(i0);
            Serial.print(" i1=");
            Serial.print(i1);
            Serial.print(" min=");
            Serial.print(chunkMin);
            Serial.print(" max=");
            Serial.print(chunkMax);
            Serial.print(" rms=");
            Serial.print(rms, 1);
            Serial.print(" mean_abs=");
            Serial.print(meanAbs, 1);
            Serial.print(" count=");
            Serial.println(chunkCount);
        }
    } else {
        float envelope = 0.0f;
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const int sample = static_cast<int>(rawBuffer[i]);
            const int absSample = sample < 0 ? -sample : sample;
            envelope = envelope * 0.95f + static_cast<float>(absSample) * 0.05f;
            Serial.print("RAW_SAMPLE i=");
            Serial.print(i);
            Serial.print(" raw=");
            Serial.print(sample);
            Serial.print(" abs=");
            Serial.print(absSample);
            Serial.print(" env=");
            Serial.println(envelope, 1);
        }
    }

    Serial.print("RAW_SUMMARY id=");
    Serial.print(captureId);
    Serial.print(" captured=");
    Serial.print(capturedSamples);
    Serial.print(" dropped=");
    Serial.print(droppedSamples);
    Serial.print(" max_raw=");
    Serial.print(maxRaw);
    Serial.print(" max_abs=");
    Serial.print(maxAbs);
    Serial.print(" max_env=");
    Serial.print(maxEnv, 1);
    Serial.println();

    if (preRingBuffer != nullptr) {
        free(preRingBuffer);
    }
}
