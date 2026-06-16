#include "../../../modes/analyzer/AnalyzerModeApp.h"
#include "../AnalyzerText.h"

#include <Arduino.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

bool waitForEmitterAck(const char* expectedPrefix, unsigned long timeoutMs);

namespace {

constexpr unsigned long kRawCaptureFlushSamples = 256;
constexpr unsigned long kRawCaptureTimeoutSlackMs = 2000;

unsigned long rawCaptureChunkSize(unsigned long sampleRateHz, unsigned long decim) {
    const unsigned long baseChunk = sampleRateHz / 20UL;
    const unsigned long decimatedChunk = decim > 0 ? baseChunk / decim : baseChunk;
    return decimatedChunk > 0 ? decimatedChunk : 1UL;
}

} // namespace

void AnalyzerApp::runRawTrigger(unsigned long toneHz,
                                unsigned long durationMs,
                                unsigned long postMs,
                                unsigned long preMs,
                                unsigned long decim,
                                bool dumpChunks,
                                bool dumpBinary,
                                bool dumpCsv) {
    stopSequenceTest();
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
    } else if (dumpCsv) {
        Serial.println("RAW_INFO dump=csv");
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

    int32_t* preRingBuffer = nullptr;
    uint32_t* preRingTimeBuffer = nullptr;
    if (preWantedSamples > 0) {
        preRingBuffer = static_cast<int32_t*>(malloc(static_cast<size_t>(preWantedSamples) * sizeof(int32_t)));
        preRingTimeBuffer = static_cast<uint32_t*>(malloc(static_cast<size_t>(preWantedSamples) * sizeof(uint32_t)));
        if (preRingBuffer == nullptr || preRingTimeBuffer == nullptr) {
            if (preRingBuffer != nullptr) {
                free(preRingBuffer);
            }
            if (preRingTimeBuffer != nullptr) {
                free(preRingTimeBuffer);
            }
            Serial.println("RAW_ERR memory=pre_ring_alloc_failed");
            return;
        }
    }

    int32_t* rawBuffer = static_cast<int32_t*>(malloc(static_cast<size_t>(maxSamples) * sizeof(int32_t)));
    uint32_t* sampleTimeMsBuffer = static_cast<uint32_t*>(malloc(static_cast<size_t>(maxSamples) * sizeof(uint32_t)));
    if (rawBuffer == nullptr || sampleTimeMsBuffer == nullptr) {
        if (preRingBuffer != nullptr) {
            free(preRingBuffer);
        }
        if (preRingTimeBuffer != nullptr) {
            free(preRingTimeBuffer);
        }
        if (sampleTimeMsBuffer != nullptr) {
            free(sampleTimeMsBuffer);
        }
        Serial.println("RAW_ERR memory=raw_buffer_alloc_failed");
        return;
    }

    unsigned long flushedSamples = 0;
    int discardedSample = 0;
    uint32_t discardedSampleTimeUs = 0;
    while (flushedSamples < kRawCaptureFlushSamples && _audioSource.readRawSample(discardedSample, discardedSampleTimeUs)) {
        ++flushedSamples;
    }

    auto claimEmitterRemoteControl = [&]() -> bool {
        sendEmitterCommand("MODE REMOTE");
        return waitForEmitterAck("OK MODE REMOTE", 1500);
    };

    if (!claimEmitterRemoteControl()) {
        Serial.println("RAW_ERR emitter_remote_claim_timeout");
        if (preRingBuffer != nullptr) {
            free(preRingBuffer);
        }
        if (preRingTimeBuffer != nullptr) {
            free(preRingTimeBuffer);
        }
        free(rawBuffer);
        free(sampleTimeMsBuffer);
        return;
    }

    const unsigned long captureId = ++_rawCaptureSequenceId;
    const unsigned long commandMs = millis();
    char emitterCommand[96];
    snprintf(emitterCommand, sizeof(emitterCommand), "CHIRP freq=%lu dur=%lu", toneHz, durationMs);
    sendEmitterCommand(emitterCommand);
    Serial2.flush();

    char emitterLineBuffer[96];
    size_t emitterLineLength = 0;
    bool emitStarted = false;
    bool emitDone = false;
    bool preWindowCopied = false;
    unsigned long emitStartMs = 0;
    unsigned long emitDoneMs = 0;
    unsigned long preCapturedTotal = 0;
    unsigned long preWindowSamples = 0;
    unsigned long preWriteIndex = 0;
    unsigned long postCaptured = 0;

    auto copyPreWindow = [&]() {
        if (preWindowCopied || preRingBuffer == nullptr || preWantedSamples == 0) {
            preWindowCopied = true;
            return;
        }

        preWindowSamples = preCapturedTotal < preWantedSamples ? preCapturedTotal : preWantedSamples;
        if (preWindowSamples == 0) {
            preWindowCopied = true;
            return;
        }

        const unsigned long startIndex = preCapturedTotal < preWantedSamples ? 0UL : preWriteIndex;
        for (unsigned long i = 0; i < preWindowSamples; ++i) {
            rawBuffer[i] = preRingBuffer[(startIndex + i) % preWantedSamples];
            sampleTimeMsBuffer[i] = preRingTimeBuffer[(startIndex + i) % preWantedSamples];
        }
        preWindowCopied = true;
    };

    auto noteEmitterLine = [&](const char* line) {
        if (line == nullptr || *line == '\0') {
            return;
        }

        const bool startLine = startsWithTokenIgnoreCase(line, "EMIT_START") || startsWithTokenIgnoreCase(line, "EMIT_DRIVE_ON");
        const bool doneLine = startsWithTokenIgnoreCase(line, "EMIT_DONE") || startsWithTokenIgnoreCase(line, "EMIT_DRIVE_OFF");
        if (startLine && !emitStarted) {
            emitStarted = true;
            emitStartMs = millis();
            copyPreWindow();
            if (!dumpBinary) {
                Serial.print("RAW_EMIT_START id=");
                Serial.print(captureId);
                Serial.print(" t_ms=");
                Serial.print(emitStartMs);
                Serial.print(" line=");
                Serial.println(line);
            }
        } else if (doneLine && !emitDone) {
            emitDone = true;
            emitDoneMs = millis();
            if (!dumpBinary) {
                Serial.print("RAW_EMIT_DONE id=");
                Serial.print(captureId);
                Serial.print(" t_ms=");
                Serial.print(emitDoneMs);
                Serial.print(" line=");
                Serial.println(line);
            }
        }
    };

    auto pumpEmitterSerial = [&]() {
        while (Serial2.available() > 0) {
            const char c = static_cast<char>(Serial2.read());
            if (c == '\r') {
                continue;
            }

            if (c == '\n') {
                emitterLineBuffer[emitterLineLength] = '\0';
                if (emitterLineLength > 0) {
                    noteEmitterLine(emitterLineBuffer);
                }
                emitterLineLength = 0;
                continue;
            }

            if (emitterLineLength < sizeof(emitterLineBuffer) - 1) {
                emitterLineBuffer[emitterLineLength++] = c;
            }
        }
    };

    auto captureSample = [&]() -> bool {
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
        if (!emitStarted) {
            if (preRingBuffer != nullptr && preWantedSamples > 0) {
                preRingBuffer[preWriteIndex] = static_cast<int32_t>(centeredSample);
                preRingTimeBuffer[preWriteIndex] = audioSamplePacket.timeMs;
                preWriteIndex = (preWriteIndex + 1UL) % preWantedSamples;
            }
            ++preCapturedTotal;
        } else if (postCaptured < postWantedSamples) {
            rawBuffer[preWindowSamples + postCaptured] = static_cast<int32_t>(centeredSample);
            sampleTimeMsBuffer[preWindowSamples + postCaptured] = audioSamplePacket.timeMs;
            ++postCaptured;
        }

        return true;
    };

    const unsigned long overallDeadlineMs = commandMs + preMs + postMs + kRawCaptureTimeoutSlackMs;
    while (millis() <= overallDeadlineMs) {
        pumpEmitterSerial();

        if (emitStarted && preWindowCopied && postCaptured >= postWantedSamples) {
            break;
        }

        if (!captureSample()) {
            delay(1);
        }
    }

    pumpEmitterSerial();
    if (!emitStarted) {
        copyPreWindow();
    }
    if (emitStarted && !emitDone) {
        const unsigned long emitDoneDeadlineMs = emitStartMs + postMs + kRawCaptureTimeoutSlackMs;
        while (millis() <= emitDoneDeadlineMs && !emitDone) {
            pumpEmitterSerial();
            if (!emitDone) {
                delay(1);
            }
        }
        pumpEmitterSerial();
    }

    const unsigned long capturedSamples = preWindowSamples + postCaptured;
    const unsigned long droppedSamples = (preWantedSamples + postWantedSamples) > capturedSamples
        ? (preWantedSamples + postWantedSamples) - capturedSamples
        : 0;

    float env = 0.0f;
    float maxEnv = 0.0f;
    int maxRaw = 0;
    int maxAbs = 0;
    for (unsigned long i = 0; i < capturedSamples; ++i) {
        const int32_t rawSample = rawBuffer[i];
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
    Serial.print(emitStarted ? emitStartMs : commandMs);
    Serial.print(" command_ms=");
    Serial.print(commandMs);
    Serial.print(" emit_seen=");
    Serial.print(emitStarted ? 1 : 0);
    Serial.print(" emit_start_ms=");
    Serial.print(emitStartMs);
    Serial.print(" emit_done_ms=");
    Serial.print(emitDoneMs);
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
    Serial.print(preWindowSamples);
    Serial.print(" post_samples=");
    Serial.print(postCaptured);
    if (dumpBinary) {
        Serial.print(" fields=raw32");
        Serial.print(" dump=bin");
        Serial.print(" samples=");
        Serial.print(capturedSamples);
        Serial.print(" bytes=");
        Serial.print(capturedSamples * sizeof(int32_t));
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
        Serial.write(reinterpret_cast<const uint8_t*>(rawBuffer), capturedSamples * sizeof(int32_t));
        Serial.println();
    } else if (dumpCsv) {
        Serial.println("ms,raw,abs,env,score,contrast,target_power,neighbor_power,total_energy,updated,age_samples");
        FreqBandStream csvBand;
        csvBand.resetState();
        csvBand.setTargetFrequencyHz(_freqBandStream.targetFrequencyHz());
        csvBand.setSampleRateHz(sampleRateHz);
        csvBand.setWindowSizeSamples(_freqBandStream.windowSizeSamples());
        csvBand.setFrequencyUpdateEverySamples(1);

        float csvEnv = 0.0f;
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const int32_t sample = rawBuffer[i];
            const int absSample = sample < 0 ? -sample : sample;
            csvEnv = csvEnv * 0.95f + static_cast<float>(absSample) * 0.05f;
            csvBand.observeCenteredSample(sample, sampleTimeMsBuffer[i]);

            Serial.print(sampleTimeMsBuffer[i]);
            Serial.print(",");
            Serial.print(sample);
            Serial.print(",");
            Serial.print(absSample);
            Serial.print(",");
            Serial.print(csvEnv, 1);
            Serial.print(",");
            Serial.print(csvBand.lastTargetBandScoreValue(), 2);
            Serial.print(",");
            Serial.print(csvBand.lastTargetBandContrastValue(), 2);
            Serial.print(",");
            Serial.print(csvBand.lastTargetBandPowerValue(), 1);
            Serial.print(",");
            Serial.print(csvBand.lastNeighborBandPowerValue(), 1);
            Serial.print(",");
            Serial.print(csvBand.lastTotalEnergyValue(), 1);
            Serial.print(",");
            Serial.print(csvBand.producedFreshPacketOnLastObserve() ? 1 : 0);
            Serial.print(",");
            Serial.println(csvBand.lastPacketAgeSamples());
        }
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
                const int32_t sample = rawBuffer[rawIndex];
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
            const long i0 = static_cast<long>(emittedStart * decim) - static_cast<long>(preWindowSamples);
            const unsigned long lastRawIndex = (emittedEnd - 1UL) * decim;
            const long i1 = static_cast<long>(lastRawIndex < capturedSamples ? lastRawIndex : (capturedSamples - 1UL)) - static_cast<long>(preWindowSamples);
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
            const int32_t sample = rawBuffer[i];
            const int absSample = sample < 0 ? -sample : sample;
            envelope = envelope * 0.95f + static_cast<float>(absSample) * 0.05f;
        Serial.print("RAW_SAMPLE i=");
        Serial.print(i);
        Serial.print(" value=");
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
    if (preRingTimeBuffer != nullptr) {
        free(preRingTimeBuffer);
    }
    if (sampleTimeMsBuffer != nullptr) {
        free(sampleTimeMsBuffer);
    }
    free(rawBuffer);
}
