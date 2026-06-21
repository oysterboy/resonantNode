#include "../../../modes/analyzer/AnalyzerModeApp.h"
#include "../AnalyzerText.h"

#include <Arduino.h>
#include <math.h>
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <string.h>

bool waitForEmitterAck(const char* expectedPrefix, unsigned long timeoutMs);

namespace {

constexpr unsigned long kRawCaptureFlushSamples = 256;
constexpr unsigned long kRawCaptureTimeoutSlackMs = 2000;
constexpr int32_t kRawCaptureFixedPointScale = 1000;

struct RawCapturePcmSample {
    uint16_t timeOffsetMs = 0;
    int32_t pcm = 0;
    float baseline = 0.0f;
};

struct RawCaptureFeatureSample {
    uint16_t timeOffsetMs = 0;
    int32_t amp = 0;
    int32_t targetScore = 0;
    uint8_t targetFresh : 1;
};

struct RawCaptureBothSample {
    uint16_t timeOffsetMs = 0;
    int32_t pcm = 0;
    int32_t amp = 0;
};

const char* rawCaptureModeName(AnalyzerApp::RawCaptureMode mode) {
    switch (mode) {
        case AnalyzerApp::RawCaptureMode::Features:
            return "feat";
        case AnalyzerApp::RawCaptureMode::Both:
            return "both";
        case AnalyzerApp::RawCaptureMode::I2s:
            return "i2s";
        case AnalyzerApp::RawCaptureMode::Pcm:
        default:
            return "pcm";
    }
}

bool rawCaptureUsesFeatureStream(AnalyzerApp::RawCaptureMode mode) {
    return mode == AnalyzerApp::RawCaptureMode::Features || mode == AnalyzerApp::RawCaptureMode::Both;
}

bool rawCaptureUsesSignalView(AnalyzerApp::RawCaptureMode mode) {
    return mode == AnalyzerApp::RawCaptureMode::Pcm || rawCaptureUsesFeatureStream(mode);
}

uint16_t rawCaptureOffsetMs(unsigned long sampleMs, unsigned long baseMs) {
    if (sampleMs <= baseMs) {
        return 0;
    }

    const unsigned long deltaMs = sampleMs - baseMs;
    return deltaMs > 0xFFFFUL ? 0xFFFFU : static_cast<uint16_t>(deltaMs);
}

int32_t rawCaptureToFixedPoint(float value) {
    return static_cast<int32_t>(lroundf(value * static_cast<float>(kRawCaptureFixedPointScale)));
}

void printRawMemoryState(const char* label) {
    Serial.print("RAW_MEM label=");
    Serial.print(label);
    Serial.print(" free_heap=");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" min_heap=");
    Serial.print(ESP.getMinFreeHeap());
    Serial.print(" largest_8bit=");
    Serial.print(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    Serial.print(" free_8bit=");
    Serial.print(heap_caps_get_free_size(MALLOC_CAP_8BIT));
    Serial.println();
}

unsigned long rawCaptureChunkSize(unsigned long sampleRateHz, unsigned long decim) {
    const unsigned long baseChunk = sampleRateHz / 20UL;
    const unsigned long decimatedChunk = decim > 0 ? baseChunk / decim : baseChunk;
    return decimatedChunk > 0 ? decimatedChunk : 1UL;
}

} // namespace

bool AnalyzerApp::runRawTrigger(unsigned long toneHz,
                                unsigned long durationMs,
                                unsigned long postMs,
                                unsigned long preMs,
                                unsigned long decim,
                                RawCaptureMode mode) {
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
    Serial.print("RAW_INFO mode=");
    Serial.print(rawCaptureModeName(mode));
    Serial.println(" output=csv");
    Serial.print("RAW_INFO row_size=");
    if (mode == RawCaptureMode::Pcm || mode == RawCaptureMode::I2s) {
        Serial.print(sizeof(RawCapturePcmSample));
    } else if (mode == RawCaptureMode::Features) {
        Serial.print(sizeof(RawCaptureFeatureSample));
    } else {
        Serial.print(sizeof(RawCaptureBothSample));
    }
    Serial.println();

    const uint32_t sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const unsigned long maxSamples = 6000UL;
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

    RawCapturePcmSample* prePcmRingBuffer = nullptr;
    RawCapturePcmSample* pcmCaptureBuffer = nullptr;
    RawCaptureFeatureSample* preFeatureRingBuffer = nullptr;
    RawCaptureFeatureSample* featureCaptureBuffer = nullptr;
    RawCaptureBothSample* preBothRingBuffer = nullptr;
    RawCaptureBothSample* bothCaptureBuffer = nullptr;

    auto allocRows = [&](size_t rowCount, size_t rowSize) -> void* {
        return rowCount > 0 ? malloc(rowCount * rowSize) : nullptr;
    };

    if (mode == RawCaptureMode::Pcm || mode == RawCaptureMode::I2s) {
        if (preWantedSamples > 0) {
            prePcmRingBuffer = static_cast<RawCapturePcmSample*>(allocRows(preWantedSamples, sizeof(RawCapturePcmSample)));
            if (prePcmRingBuffer == nullptr) {
                Serial.println("RAW_ERR memory=pre_ring_alloc_failed");
                printRawMemoryState("pre_ring");
                return false;
            }
        }

        pcmCaptureBuffer = static_cast<RawCapturePcmSample*>(allocRows(maxSamples, sizeof(RawCapturePcmSample)));
        if (pcmCaptureBuffer == nullptr) {
            if (prePcmRingBuffer != nullptr) {
                free(prePcmRingBuffer);
            }
            Serial.println("RAW_ERR memory=raw_buffer_alloc_failed");
            printRawMemoryState("raw_buffer");
            return false;
        }
    } else if (mode == RawCaptureMode::Features) {
        if (preWantedSamples > 0) {
            preFeatureRingBuffer = static_cast<RawCaptureFeatureSample*>(allocRows(preWantedSamples, sizeof(RawCaptureFeatureSample)));
            if (preFeatureRingBuffer == nullptr) {
                Serial.println("RAW_ERR memory=pre_ring_alloc_failed");
                printRawMemoryState("pre_ring");
                return false;
            }
        }

        featureCaptureBuffer = static_cast<RawCaptureFeatureSample*>(allocRows(maxSamples, sizeof(RawCaptureFeatureSample)));
        if (featureCaptureBuffer == nullptr) {
            if (preFeatureRingBuffer != nullptr) {
                free(preFeatureRingBuffer);
            }
            Serial.println("RAW_ERR memory=raw_buffer_alloc_failed");
            printRawMemoryState("raw_buffer");
            return false;
        }
    } else {
        if (preWantedSamples > 0) {
            preBothRingBuffer = static_cast<RawCaptureBothSample*>(allocRows(preWantedSamples, sizeof(RawCaptureBothSample)));
            if (preBothRingBuffer == nullptr) {
                Serial.println("RAW_ERR memory=pre_ring_alloc_failed");
                printRawMemoryState("pre_ring");
                return false;
            }
        }

        bothCaptureBuffer = static_cast<RawCaptureBothSample*>(allocRows(maxSamples, sizeof(RawCaptureBothSample)));
        if (bothCaptureBuffer == nullptr) {
            if (preBothRingBuffer != nullptr) {
                free(preBothRingBuffer);
            }
            Serial.println("RAW_ERR memory=raw_buffer_alloc_failed");
            printRawMemoryState("raw_buffer");
            return false;
        }
    }

    unsigned long flushedSamples = 0;
    int discardedSample = 0;
    uint32_t discardedSampleTimeUs = 0;
    const bool useSignalView = rawCaptureUsesSignalView(mode);
    while (flushedSamples < kRawCaptureFlushSamples &&
           (useSignalView ? _audioSource.readSample(discardedSample, discardedSampleTimeUs)
                          : _audioSource.readRawSample(discardedSample, discardedSampleTimeUs))) {
        ++flushedSamples;
    }

    auto claimEmitterRemoteControl = [&]() -> bool {
        sendEmitterCommand("MODE REMOTE");
        return waitForEmitterAck("OK MODE REMOTE", 1500);
    };

    if (!claimEmitterRemoteControl()) {
        Serial.println("RAW_ERR emitter_remote_claim_timeout");
        printRawMemoryState("emitter_timeout");
        if (prePcmRingBuffer != nullptr) {
            free(prePcmRingBuffer);
        }
        if (preFeatureRingBuffer != nullptr) {
            free(preFeatureRingBuffer);
        }
        if (preBothRingBuffer != nullptr) {
            free(preBothRingBuffer);
        }
        if (pcmCaptureBuffer != nullptr) {
            free(pcmCaptureBuffer);
        }
        if (featureCaptureBuffer != nullptr) {
            free(featureCaptureBuffer);
        }
        if (bothCaptureBuffer != nullptr) {
            free(bothCaptureBuffer);
        }
        return false;
    }

    const unsigned long captureId = ++_rawCaptureSequenceId;
    const unsigned long commandMs = millis();
    const unsigned long captureBaseMs = commandMs;
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
    int64_t rawWindowSum = 0;
    uint64_t rawWindowAbsSum = 0;
    int32_t rawWindowMin = INT32_MAX;
    int32_t rawWindowMax = INT32_MIN;

    auto copyPreWindow = [&]() {
        if (preWindowCopied || preWantedSamples == 0) {
            preWindowCopied = true;
            return;
        }

        preWindowSamples = preCapturedTotal < preWantedSamples ? preCapturedTotal : preWantedSamples;
        if (preWindowSamples == 0) {
            preWindowCopied = true;
            return;
        }

        const unsigned long startIndex = preCapturedTotal < preWantedSamples ? 0UL : preWriteIndex;
        if ((mode == RawCaptureMode::Pcm || mode == RawCaptureMode::I2s) && prePcmRingBuffer != nullptr && pcmCaptureBuffer != nullptr) {
            for (unsigned long i = 0; i < preWindowSamples; ++i) {
                pcmCaptureBuffer[i] = prePcmRingBuffer[(startIndex + i) % preWantedSamples];
            }
        } else if (mode == RawCaptureMode::Features && preFeatureRingBuffer != nullptr && featureCaptureBuffer != nullptr) {
            for (unsigned long i = 0; i < preWindowSamples; ++i) {
                featureCaptureBuffer[i] = preFeatureRingBuffer[(startIndex + i) % preWantedSamples];
            }
        } else if (mode == RawCaptureMode::Both && preBothRingBuffer != nullptr && bothCaptureBuffer != nullptr) {
            for (unsigned long i = 0; i < preWindowSamples; ++i) {
                bothCaptureBuffer[i] = preBothRingBuffer[(startIndex + i) % preWantedSamples];
            }
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
            if (mode != RawCaptureMode::Pcm && mode != RawCaptureMode::I2s) {
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
            if (mode != RawCaptureMode::Pcm && mode != RawCaptureMode::I2s) {
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
        int sourceSample = 0;
        uint32_t sampleTimeUs = 0;
        if (!(useSignalView ? _audioSource.readSample(sourceSample, sampleTimeUs)
                            : _audioSource.readRawSample(sourceSample, sampleTimeUs))) {
            return false;
        }

        const unsigned long sampleTimeMs = sampleTimeUs / 1000UL;
        const uint16_t sampleTimeOffsetMs = rawCaptureOffsetMs(sampleTimeMs, captureBaseMs);
        const int32_t signedRawSample = static_cast<int32_t>(sourceSample);
        rawWindowSum += static_cast<int64_t>(signedRawSample);
        rawWindowAbsSum += static_cast<uint64_t>(signedRawSample < 0 ? -static_cast<int64_t>(signedRawSample) : static_cast<int64_t>(signedRawSample));
        if (signedRawSample < rawWindowMin) {
            rawWindowMin = signedRawSample;
        }
        if (signedRawSample > rawWindowMax) {
            rawWindowMax = signedRawSample;
        }
        if (useSignalView) {
            AudioSamplePacket audioSamplePacket = {};
            _audioSignal.update(sourceSample, sampleTimeUs, audioSamplePacket);

            if (mode == RawCaptureMode::Pcm) {
                if (!emitStarted) {
                    if (prePcmRingBuffer != nullptr) {
                        prePcmRingBuffer[preWriteIndex].timeOffsetMs = sampleTimeOffsetMs;
                        prePcmRingBuffer[preWriteIndex].pcm = audioSamplePacket.rawAudioValue;
                        prePcmRingBuffer[preWriteIndex].baseline = audioSamplePacket.baseline;
                        preWriteIndex = (preWriteIndex + 1UL) % preWantedSamples;
                    }
                    ++preCapturedTotal;
                } else if (postCaptured < postWantedSamples) {
                    if (pcmCaptureBuffer != nullptr) {
                        pcmCaptureBuffer[preWindowSamples + postCaptured].timeOffsetMs = sampleTimeOffsetMs;
                        pcmCaptureBuffer[preWindowSamples + postCaptured].pcm = audioSamplePacket.rawAudioValue;
                        pcmCaptureBuffer[preWindowSamples + postCaptured].baseline = audioSamplePacket.baseline;
                    }
                    ++postCaptured;
                }
                return true;
            }

            _freqBandStream.observeCenteredSample(audioSamplePacket.baselineCorrectedValue, audioSamplePacket.timeMs);
            const detection::FrequencyBandMeasurementPacket frequencyEvidence = captureFrequencyMeasurementPacket(audioSamplePacket);
            _detection.observeFrame(audioSamplePacket, frequencyEvidence, audioSamplePacket.timeMs);

            const detection::FeatureHistory& featureHistory = _detection.featureHistory();
            const int32_t ampFixed = rawCaptureToFixedPoint(featureHistory.latestValue(detection::FeatureStreamId::AmpEnvelope));

            if (!emitStarted) {
                if (mode == RawCaptureMode::Features && preFeatureRingBuffer != nullptr) {
                    preFeatureRingBuffer[preWriteIndex].timeOffsetMs = sampleTimeOffsetMs;
                    preFeatureRingBuffer[preWriteIndex].amp = ampFixed;
                    preFeatureRingBuffer[preWriteIndex].targetScore = rawCaptureToFixedPoint(featureHistory.latestValue(detection::FeatureStreamId::FrequencyTargetBand));
                    preFeatureRingBuffer[preWriteIndex].targetFresh = frequencyEvidence.fresh ? 1U : 0U;
                    preWriteIndex = (preWriteIndex + 1UL) % preWantedSamples;
                } else if (mode == RawCaptureMode::Both && preBothRingBuffer != nullptr) {
                    preBothRingBuffer[preWriteIndex].timeOffsetMs = sampleTimeOffsetMs;
                    preBothRingBuffer[preWriteIndex].pcm = static_cast<int32_t>(sourceSample);
                    preBothRingBuffer[preWriteIndex].amp = ampFixed;
                    preWriteIndex = (preWriteIndex + 1UL) % preWantedSamples;
                }
                ++preCapturedTotal;
            } else if (postCaptured < postWantedSamples) {
                if (mode == RawCaptureMode::Features && featureCaptureBuffer != nullptr) {
                    featureCaptureBuffer[preWindowSamples + postCaptured].timeOffsetMs = sampleTimeOffsetMs;
                    featureCaptureBuffer[preWindowSamples + postCaptured].amp = ampFixed;
                    featureCaptureBuffer[preWindowSamples + postCaptured].targetScore = rawCaptureToFixedPoint(featureHistory.latestValue(detection::FeatureStreamId::FrequencyTargetBand));
                    featureCaptureBuffer[preWindowSamples + postCaptured].targetFresh = frequencyEvidence.fresh ? 1U : 0U;
                } else if (mode == RawCaptureMode::Both && bothCaptureBuffer != nullptr) {
                    bothCaptureBuffer[preWindowSamples + postCaptured].timeOffsetMs = sampleTimeOffsetMs;
                    bothCaptureBuffer[preWindowSamples + postCaptured].pcm = static_cast<int32_t>(sourceSample);
                    bothCaptureBuffer[preWindowSamples + postCaptured].amp = ampFixed;
                }
                ++postCaptured;
            }
            return true;
        }

        if (!emitStarted) {
            if (mode == RawCaptureMode::I2s && prePcmRingBuffer != nullptr) {
                prePcmRingBuffer[preWriteIndex].timeOffsetMs = sampleTimeOffsetMs;
                prePcmRingBuffer[preWriteIndex].pcm = static_cast<int32_t>(sourceSample);
                preWriteIndex = (preWriteIndex + 1UL) % preWantedSamples;
            }
            ++preCapturedTotal;
        } else if (postCaptured < postWantedSamples) {
            if (mode == RawCaptureMode::I2s && pcmCaptureBuffer != nullptr) {
                pcmCaptureBuffer[preWindowSamples + postCaptured].timeOffsetMs = sampleTimeOffsetMs;
                pcmCaptureBuffer[preWindowSamples + postCaptured].pcm = static_cast<int32_t>(sourceSample);
            }
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

    const unsigned long requestedSamples = preWantedSamples + postWantedSamples;
    const unsigned long capturedSamples = preWindowSamples + postCaptured;
    const unsigned long droppedSamples = requestedSamples > capturedSamples
        ? requestedSamples - capturedSamples
        : 0;
    const unsigned long unusedCapacitySamples = maxSamples > capturedSamples
        ? maxSamples - capturedSamples
        : 0;
    const float rawWindowMean = capturedSamples > 0 ? static_cast<float>(rawWindowSum) / static_cast<float>(capturedSamples) : 0.0f;
    const float rawWindowAbsMean = capturedSamples > 0 ? static_cast<float>(rawWindowAbsSum) / static_cast<float>(capturedSamples) : 0.0f;
    const int32_t rawWindowMinOut = capturedSamples > 0 ? rawWindowMin : 0;
    const int32_t rawWindowMaxOut = capturedSamples > 0 ? rawWindowMax : 0;

    float maxAmp = 0.0f;
    int32_t maxPcmAbs = 0;
    for (unsigned long i = 0; i < capturedSamples; ++i) {
        if (mode == RawCaptureMode::Pcm || mode == RawCaptureMode::I2s) {
            const RawCapturePcmSample& sample = pcmCaptureBuffer[i];
            const int32_t absPcm = sample.pcm < 0 ? -sample.pcm : sample.pcm;
            if (absPcm > maxPcmAbs) {
                maxPcmAbs = absPcm;
            }
        } else if (mode == RawCaptureMode::Features) {
            const RawCaptureFeatureSample& sample = featureCaptureBuffer[i];
            const float ampValue = static_cast<float>(sample.amp) / static_cast<float>(kRawCaptureFixedPointScale);
            if (ampValue > maxAmp) {
                maxAmp = ampValue;
            }
        } else {
            const RawCaptureBothSample& sample = bothCaptureBuffer[i];
            const int32_t absPcm = sample.pcm < 0 ? -sample.pcm : sample.pcm;
            if (absPcm > maxPcmAbs) {
                maxPcmAbs = absPcm;
            }
            const float ampValue = static_cast<float>(sample.amp) / static_cast<float>(kRawCaptureFixedPointScale);
            if (ampValue > maxAmp) {
                maxAmp = ampValue;
            }
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
    Serial.print(" mode=");
    Serial.print(rawCaptureModeName(mode));
    Serial.print(" output=csv");
    if (mode == RawCaptureMode::Pcm) {
        Serial.print(" fields=ms,pcm,baseline");
    } else if (mode == RawCaptureMode::I2s) {
        Serial.print(" fields=ms,pcm");
    } else if (mode == RawCaptureMode::Features) {
        Serial.print(" fields=ms,amp,env,target_score,target_fresh");
    } else {
        Serial.print(" fields=ms,amp,pcm");
    }
    Serial.println();

    if (mode == RawCaptureMode::Pcm) {
        Serial.println("ms,pcm,baseline,centered");
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const RawCapturePcmSample& sample = pcmCaptureBuffer[i];
            Serial.print(static_cast<unsigned long>(sample.timeOffsetMs) + captureBaseMs);
            Serial.print(",");
            Serial.print(sample.pcm);
            Serial.print(",");
            Serial.print(sample.baseline, 1);
            Serial.print(",");
            Serial.println(sample.pcm - static_cast<int32_t>(sample.baseline));
        }
    } else if (mode == RawCaptureMode::I2s) {
        Serial.println("ms,pcm");
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const RawCapturePcmSample& sample = pcmCaptureBuffer[i];
            Serial.print(static_cast<unsigned long>(sample.timeOffsetMs) + captureBaseMs);
            Serial.print(",");
            Serial.println(sample.pcm);
        }
    } else if (mode == RawCaptureMode::Features) {
        Serial.println("ms,amp,env,target_score,target_fresh");
        float csvEnv = 0.0f;
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const RawCaptureFeatureSample& sample = featureCaptureBuffer[i];
            const float ampValue = static_cast<float>(sample.amp) / static_cast<float>(kRawCaptureFixedPointScale);
            const float targetScoreValue = static_cast<float>(sample.targetScore) / static_cast<float>(kRawCaptureFixedPointScale);
            csvEnv = csvEnv * 0.95f + ampValue * 0.05f;
            Serial.print(static_cast<unsigned long>(sample.timeOffsetMs) + captureBaseMs);
            Serial.print(",");
            Serial.print(ampValue, 1);
            Serial.print(",");
            Serial.print(csvEnv, 1);
            Serial.print(",");
            Serial.print(targetScoreValue, 2);
            Serial.print(",");
            Serial.println(sample.targetFresh ? 1 : 0);
        }
    } else {
        Serial.println("ms,amp,pcm");
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const RawCaptureBothSample& sample = bothCaptureBuffer[i];
            Serial.print(static_cast<unsigned long>(sample.timeOffsetMs) + captureBaseMs);
            Serial.print(",");
            Serial.print(static_cast<float>(sample.amp) / static_cast<float>(kRawCaptureFixedPointScale), 1);
            Serial.print(",");
            Serial.println(sample.pcm);
        }
    }

    Serial.print("RAW_SUMMARY id=");
    Serial.print(captureId);
    Serial.print(" requested=");
    Serial.print(requestedSamples);
    Serial.print(" captured=");
    Serial.print(capturedSamples);
    Serial.print(" dropped=");
    Serial.print(droppedSamples);
    Serial.print(" unused_capacity=");
    Serial.print(unusedCapacitySamples);
    Serial.print(" max_pcm_abs=");
    Serial.print(maxPcmAbs);
    if (rawCaptureUsesFeatureStream(mode)) {
        Serial.print(" max_amp=");
        Serial.print(maxAmp, 1);
    }
    Serial.print(" raw_mean=");
    Serial.print(rawWindowMean, 1);
    Serial.print(" raw_abs_mean=");
    Serial.print(rawWindowAbsMean, 1);
    Serial.print(" raw_min=");
    Serial.print(rawWindowMinOut);
    Serial.print(" raw_max=");
    Serial.print(rawWindowMaxOut);
    Serial.print(" baseline=");
    Serial.print(_audioSignal.baseline(), 1);
    Serial.println();

    if (prePcmRingBuffer != nullptr) {
        free(prePcmRingBuffer);
    }
    if (preFeatureRingBuffer != nullptr) {
        free(preFeatureRingBuffer);
    }
    if (preBothRingBuffer != nullptr) {
        free(preBothRingBuffer);
    }
    if (pcmCaptureBuffer != nullptr) {
        free(pcmCaptureBuffer);
    }
    if (featureCaptureBuffer != nullptr) {
        free(featureCaptureBuffer);
    }
    if (bothCaptureBuffer != nullptr) {
        free(bothCaptureBuffer);
    }
    return true;
}
