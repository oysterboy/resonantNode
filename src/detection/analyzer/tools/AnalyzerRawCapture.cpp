#include "../../../modes/analyzer/AnalyzerModeApp.h"
#include "../../../audio/AudioPcm.h"
#include "../AnalyzerText.h"
#include "../../../app/SerialLine.h"

#include <Arduino.h>
#include <math.h>
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <string.h>
#include "../../../app/TimeBase.h"
#include "../../../app/TimingUtils.h"
#include "../../../app/SerialOut.h"

bool waitForEmitterAck(const char* expectedPrefix, unsigned long timeoutMs);

namespace {

constexpr unsigned long kRawCaptureFlushSamples = 256;
constexpr unsigned long kRawCaptureTimeoutSlackMs = 2000;

struct RawCapturePcmSample {
    uint16_t timeOffsetMs = 0;
    int32_t pcm = 0;
    float baseline = 0.0f;
};

struct RawCaptureFeatureSample {
    uint16_t timeOffsetMs = 0;
    uint16_t ampStrength = 0;
    uint16_t envStrength = 0;
    uint16_t frequencyScore = 0;
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
    if (timing::atOrBefore(sampleMs, baseMs)) {
        return 0;
    }

    const unsigned long deltaMs = sampleMs - baseMs;
    return deltaMs > 0xFFFFUL ? 0xFFFFU : static_cast<uint16_t>(deltaMs);
}

void printRawMemoryState(const char* label) {
    serialOut().print("RAW_MEM label=");
    serialOut().print(label);
    serialOut().print(" free_heap=");
    serialOut().print(ESP.getFreeHeap());
    serialOut().print(" min_heap=");
    serialOut().print(ESP.getMinFreeHeap());
    serialOut().print(" largest_8bit=");
    serialOut().print(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    serialOut().print(" free_8bit=");
    serialOut().print(heap_caps_get_free_size(MALLOC_CAP_8BIT));
    serialOut().println();
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
                                RawCaptureMode mode,
                                const RawTriggerShape& shape) {
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
    serialOut().print("RAW_INFO mode=");
    serialOut().print(rawCaptureModeName(mode));
    serialOut().println(" output=csv");
    serialOut().print("RAW_INFO row_size=");
    if (mode == RawCaptureMode::Pcm || mode == RawCaptureMode::I2s) {
        serialOut().print(sizeof(RawCapturePcmSample));
    } else if (mode == RawCaptureMode::Features) {
        serialOut().print(sizeof(RawCaptureFeatureSample));
    } else {
        serialOut().print(sizeof(RawCaptureBothSample));
    }
    serialOut().println();

    const uint32_t sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const unsigned long maxSamples = 6000UL;
    // mode=i2s with decim=N keeps every Nth word, so the fixed row budget
    // covers N times longer (e.g. a clap test over seconds). Other modes
    // keep using decim for output chunking only.
    const unsigned long i2sKeepEvery = (mode == RawCaptureMode::I2s && decim > 1) ? decim : 1UL;
    const uint32_t storedRateHz = sampleRateHz / i2sKeepEvery;
    unsigned long preWantedSamples = static_cast<unsigned long>((static_cast<uint64_t>(preMs) * static_cast<uint64_t>(storedRateHz)) / 1000ULL);
    unsigned long postWantedSamples = static_cast<unsigned long>((static_cast<uint64_t>(postMs) * static_cast<uint64_t>(storedRateHz)) / 1000ULL);
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
                serialOut().println("RAW_ERR memory=pre_ring_alloc_failed");
                printRawMemoryState("pre_ring");
                return false;
            }
        }

        pcmCaptureBuffer = static_cast<RawCapturePcmSample*>(allocRows(maxSamples, sizeof(RawCapturePcmSample)));
        if (pcmCaptureBuffer == nullptr) {
            if (prePcmRingBuffer != nullptr) {
                free(prePcmRingBuffer);
            }
            serialOut().println("RAW_ERR memory=raw_buffer_alloc_failed");
            printRawMemoryState("raw_buffer");
            return false;
        }
    } else if (mode == RawCaptureMode::Features) {
        if (preWantedSamples > 0) {
            preFeatureRingBuffer = static_cast<RawCaptureFeatureSample*>(allocRows(preWantedSamples, sizeof(RawCaptureFeatureSample)));
            if (preFeatureRingBuffer == nullptr) {
                serialOut().println("RAW_ERR memory=pre_ring_alloc_failed");
                printRawMemoryState("pre_ring");
                return false;
            }
        }

        featureCaptureBuffer = static_cast<RawCaptureFeatureSample*>(allocRows(maxSamples, sizeof(RawCaptureFeatureSample)));
        if (featureCaptureBuffer == nullptr) {
            if (preFeatureRingBuffer != nullptr) {
                free(preFeatureRingBuffer);
            }
            serialOut().println("RAW_ERR memory=raw_buffer_alloc_failed");
            printRawMemoryState("raw_buffer");
            return false;
        }
    } else {
        if (preWantedSamples > 0) {
            preBothRingBuffer = static_cast<RawCaptureBothSample*>(allocRows(preWantedSamples, sizeof(RawCaptureBothSample)));
            if (preBothRingBuffer == nullptr) {
                serialOut().println("RAW_ERR memory=pre_ring_alloc_failed");
                printRawMemoryState("pre_ring");
                return false;
            }
        }

        bothCaptureBuffer = static_cast<RawCaptureBothSample*>(allocRows(maxSamples, sizeof(RawCaptureBothSample)));
        if (bothCaptureBuffer == nullptr) {
            if (preBothRingBuffer != nullptr) {
                free(preBothRingBuffer);
            }
            serialOut().println("RAW_ERR memory=raw_buffer_alloc_failed");
            printRawMemoryState("raw_buffer");
            return false;
        }
    }

    unsigned long flushedSamples = 0;
    int discardedSample = 0;
    uint64_t discardedSampleTimeUs = 0;
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
        serialOut().println("RAW_ERR emitter_remote_claim_timeout");
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
    const unsigned long commandMs = timebase::nowMs();
    const unsigned long captureBaseMs = commandMs;
    // One CHIRP, or the same PULSES line SEQ start sends (trial=0: no trial
    // bookkeeping on the Emitter side); EMIT_START/EMIT_DONE bracket either.
    char emitterCommand[128];
    if (shape.pulseCount > 0) {
        int written = snprintf(emitterCommand, sizeof(emitterCommand), "PULSES %lu %lu %lu trial=0 freq=%lu",
                               shape.pulseCount, durationMs, shape.pulseGapMs, toneHz);
        if (shape.hop[0] != '\0' && written > 0 && static_cast<size_t>(written) < sizeof(emitterCommand)) {
            snprintf(emitterCommand + written, sizeof(emitterCommand) - written, " hop=%s", shape.hop);
        }
    } else if (shape.sweepToHz != 0) {
        snprintf(emitterCommand, sizeof(emitterCommand), "CHIRP freq=%lu dur=%lu sweep=%lu", toneHz, durationMs, shape.sweepToHz);
    } else {
        snprintf(emitterCommand, sizeof(emitterCommand), "CHIRP freq=%lu dur=%lu", toneHz, durationMs);
    }
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
    // Every sample read, stored or not (pre=0 drops the pre-emit ones); the
    // mean divides by this, not by the stored count (issue #31).
    unsigned long rawWindowCount = 0;
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
            emitStartMs = timebase::nowMs();
            copyPreWindow();
            if (mode != RawCaptureMode::Pcm && mode != RawCaptureMode::I2s) {
                serialOut().print("RAW_EMIT_START id=");
                serialOut().print(captureId);
                serialOut().print(" t_ms=");
                serialOut().print(emitStartMs);
                serialOut().print(" line=");
                serialOut().println(line);
            }
        } else if (doneLine && !emitDone) {
            emitDone = true;
            emitDoneMs = timebase::nowMs();
            if (mode != RawCaptureMode::Pcm && mode != RawCaptureMode::I2s) {
                serialOut().print("RAW_EMIT_DONE id=");
                serialOut().print(captureId);
                serialOut().print(" t_ms=");
                serialOut().print(emitDoneMs);
                serialOut().print(" line=");
                serialOut().println(line);
            }
        }
    };

    auto pumpEmitterSerial = [&]() {
        while (Serial2.available() > 0) {
            const char c = static_cast<char>(Serial2.read());
            if (serial_line::dropByte(c)) {
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

    unsigned long i2sDecimCounter = 0;
    timebase::MsFromUs64 rawMsFromUs;
    auto captureSample = [&]() -> bool {
        int sourceSample = 0;
        uint64_t sampleTimeUs64 = 0;
        if (!(useSignalView ? _audioSource.readSample(sourceSample, sampleTimeUs64)
                            : _audioSource.readRawSample(sourceSample, sampleTimeUs64))) {
            return false;
        }
        if (i2sKeepEvery > 1 && (i2sDecimCounter++ % i2sKeepEvery) != 0) {
            return true;
        }

        const unsigned long sampleTimeMs = rawMsFromUs(sampleTimeUs64);
        const uint16_t sampleTimeOffsetMs = rawCaptureOffsetMs(sampleTimeMs, captureBaseMs);
        const int32_t signedRawSample = static_cast<int32_t>(sourceSample);
        rawWindowSum += static_cast<int64_t>(signedRawSample);
        ++rawWindowCount;
        rawWindowAbsSum += static_cast<uint64_t>(signedRawSample < 0 ? -static_cast<int64_t>(signedRawSample) : static_cast<int64_t>(signedRawSample));
        if (signedRawSample < rawWindowMin) {
            rawWindowMin = signedRawSample;
        }
        if (signedRawSample > rawWindowMax) {
            rawWindowMax = signedRawSample;
        }
        if (useSignalView) {
            AudioSamplePacket audioSamplePacket = {};
            _audioSignal.update(sourceSample, sampleTimeUs64, audioSamplePacket);

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

            const uint16_t ampStrength = audioSamplePacket.audioMagnitudeValue;
            const uint16_t envStrength = audioSamplePacket.smoothedLevel;
            const uint16_t frequencyScore = frequencyEvidence.targetBandValue;

            if (!emitStarted) {
                if (mode == RawCaptureMode::Features && preFeatureRingBuffer != nullptr) {
                    preFeatureRingBuffer[preWriteIndex].timeOffsetMs = sampleTimeOffsetMs;
                    preFeatureRingBuffer[preWriteIndex].ampStrength = ampStrength;
                    preFeatureRingBuffer[preWriteIndex].envStrength = envStrength;
                    preFeatureRingBuffer[preWriteIndex].frequencyScore = frequencyScore;
                    preFeatureRingBuffer[preWriteIndex].targetFresh = frequencyEvidence.fresh ? 1U : 0U;
                    preWriteIndex = (preWriteIndex + 1UL) % preWantedSamples;
                } else if (mode == RawCaptureMode::Both && preBothRingBuffer != nullptr) {
                    preBothRingBuffer[preWriteIndex].timeOffsetMs = sampleTimeOffsetMs;
                    preBothRingBuffer[preWriteIndex].pcm = static_cast<int32_t>(sourceSample);
                    preBothRingBuffer[preWriteIndex].amp = ampStrength;
                    preWriteIndex = (preWriteIndex + 1UL) % preWantedSamples;
                }
                ++preCapturedTotal;
            } else if (postCaptured < postWantedSamples) {
                if (mode == RawCaptureMode::Features && featureCaptureBuffer != nullptr) {
                    featureCaptureBuffer[preWindowSamples + postCaptured].timeOffsetMs = sampleTimeOffsetMs;
                    featureCaptureBuffer[preWindowSamples + postCaptured].ampStrength = ampStrength;
                    featureCaptureBuffer[preWindowSamples + postCaptured].envStrength = envStrength;
                    featureCaptureBuffer[preWindowSamples + postCaptured].frequencyScore = frequencyScore;
                    featureCaptureBuffer[preWindowSamples + postCaptured].targetFresh = frequencyEvidence.fresh ? 1U : 0U;
                } else if (mode == RawCaptureMode::Both && bothCaptureBuffer != nullptr) {
                    bothCaptureBuffer[preWindowSamples + postCaptured].timeOffsetMs = sampleTimeOffsetMs;
                    bothCaptureBuffer[preWindowSamples + postCaptured].pcm = static_cast<int32_t>(sourceSample);
                    bothCaptureBuffer[preWindowSamples + postCaptured].amp = ampStrength;
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
    while (timing::atOrBefore(timebase::nowMs(), overallDeadlineMs)) {
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
        while (timing::atOrBefore(timebase::nowMs(), emitDoneDeadlineMs) && !emitDone) {
            pumpEmitterSerial();
            if (!emitDone) {
                delay(1);
            }
        }
        pumpEmitterSerial();
    }

    const unsigned long requestedSamples = preWantedSamples + postWantedSamples;
    const unsigned long capturedSamples = preWindowSamples + postCaptured;
    const unsigned long unusedCapacitySamples = maxSamples > capturedSamples
        ? maxSamples - capturedSamples
        : 0;
    const float rawWindowMean = rawWindowCount > 0 ? static_cast<float>(rawWindowSum) / static_cast<float>(rawWindowCount) : 0.0f;
    const float rawWindowAbsMean = rawWindowCount > 0 ? static_cast<float>(rawWindowAbsSum) / static_cast<float>(rawWindowCount) : 0.0f;
    const int32_t rawWindowMinOut = rawWindowCount > 0 ? rawWindowMin : 0;
    const int32_t rawWindowMaxOut = rawWindowCount > 0 ? rawWindowMax : 0;

    uint16_t maxAmp = 0;
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
            if (sample.ampStrength > maxAmp) {
                maxAmp = sample.ampStrength;
            }
        } else {
            const RawCaptureBothSample& sample = bothCaptureBuffer[i];
            const int32_t absPcm = sample.pcm < 0 ? -sample.pcm : sample.pcm;
            if (absPcm > maxPcmAbs) {
                maxPcmAbs = absPcm;
            }
            if (sample.amp > maxAmp) {
                maxAmp = static_cast<uint16_t>(sample.amp);
            }
        }
    }

    serialOut().print("RAW_BEGIN id=");
    serialOut().print(captureId);
    serialOut().print(" sr=");
    serialOut().print(storedRateHz);
    serialOut().print(" trigger_ms=");
    serialOut().print(emitStarted ? emitStartMs : commandMs);
    serialOut().print(" command_ms=");
    serialOut().print(commandMs);
    serialOut().print(" emit_seen=");
    serialOut().print(emitStarted ? 1 : 0);
    serialOut().print(" emit_start_ms=");
    serialOut().print(emitStartMs);
    serialOut().print(" emit_done_ms=");
    serialOut().print(emitDoneMs);
    serialOut().print(" f=");
    serialOut().print(toneHz);
    serialOut().print(" dur=");
    serialOut().print(durationMs);
    if (shape.pulseCount > 0) {
        serialOut().print(" pulses=");
        serialOut().print(shape.pulseCount);
        serialOut().print(" gap=");
        serialOut().print(shape.pulseGapMs);
        if (shape.hop[0] != '\0') {
            serialOut().print(" hop=");
            serialOut().print(shape.hop);
        }
    } else if (shape.sweepToHz != 0) {
        serialOut().print(" sweep=");
        serialOut().print(shape.sweepToHz);
    }
    serialOut().print(" pre_ms=");
    serialOut().print(preMs);
    serialOut().print(" post_ms=");
    serialOut().print(postMs);
    // Which PCM conditioning produced the stored stream (mode=i2s stores the
    // raw PCM, every other mode what AudioSignal received), so a transcript
    // used as a host fixture says which chain it represents.
    serialOut().print(" preprocess=");
    serialOut().print(runtime::kPcmPreprocessMode == runtime::PcmPreprocessMode::DcBlocker ? "dc"
                 : runtime::kPcmPreprocessMode == runtime::PcmPreprocessMode::FirstDifference ? "fd" : "none");
    serialOut().print(" decim=");
    serialOut().print(decim);
    serialOut().print(" pre_samples=");
    serialOut().print(preWindowSamples);
    serialOut().print(" post_samples=");
    serialOut().print(postCaptured);
    serialOut().print(" mode=");
    serialOut().print(rawCaptureModeName(mode));
    serialOut().print(" output=csv");
    if (mode == RawCaptureMode::Pcm) {
        serialOut().print(" fields=ms,pcm,baseline");
    } else if (mode == RawCaptureMode::I2s) {
        serialOut().print(" fields=ms,pcm");
    } else if (mode == RawCaptureMode::Features) {
        serialOut().print(" fields=ms,amp_strength,env_strength,freq_score,freq_fresh");
    } else {
        serialOut().print(" fields=ms,amp,pcm");
    }
    serialOut().println();

    if (mode == RawCaptureMode::Pcm) {
        serialOut().println("ms,pcm,baseline,centered");
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const RawCapturePcmSample& sample = pcmCaptureBuffer[i];
            serialOut().print(static_cast<unsigned long>(sample.timeOffsetMs) + captureBaseMs);
            serialOut().print(",");
            serialOut().print(sample.pcm);
            serialOut().print(",");
            serialOut().print(sample.baseline, 1);
            serialOut().print(",");
            serialOut().println(sample.pcm - static_cast<int32_t>(sample.baseline));
        }
    } else if (mode == RawCaptureMode::I2s) {
        serialOut().println("ms,pcm");
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const RawCapturePcmSample& sample = pcmCaptureBuffer[i];
            serialOut().print(static_cast<unsigned long>(sample.timeOffsetMs) + captureBaseMs);
            serialOut().print(",");
            serialOut().println(sample.pcm);
        }
    } else if (mode == RawCaptureMode::Features) {
        serialOut().println("ms,amp_strength,env_strength,freq_score,freq_fresh");
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const RawCaptureFeatureSample& sample = featureCaptureBuffer[i];
            serialOut().print(static_cast<unsigned long>(sample.timeOffsetMs) + captureBaseMs);
            serialOut().print(",");
            serialOut().print(sample.ampStrength);
            serialOut().print(",");
            serialOut().print(sample.envStrength);
            serialOut().print(",");
            serialOut().print(sample.frequencyScore);
            serialOut().print(",");
            serialOut().println(sample.targetFresh ? 1 : 0);
        }
    } else {
        serialOut().println("ms,amp,pcm");
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const RawCaptureBothSample& sample = bothCaptureBuffer[i];
            serialOut().print(static_cast<unsigned long>(sample.timeOffsetMs) + captureBaseMs);
            serialOut().print(",");
            serialOut().print(sample.amp);
            serialOut().print(",");
            serialOut().println(sample.pcm);
        }
    }

    serialOut().print("RAW_SUMMARY id=");
    serialOut().print(captureId);
    serialOut().print(" requested=");
    serialOut().print(requestedSamples);
    serialOut().print(" captured=");
    serialOut().print(capturedSamples);
    serialOut().print(" unused_capacity=");
    serialOut().print(unusedCapacitySamples);
    serialOut().print(" max_pcm_abs=");
    serialOut().print(maxPcmAbs);
    if (rawCaptureUsesFeatureStream(mode)) {
        serialOut().print(" max_amp=");
        serialOut().print(maxAmp);
    }
    serialOut().print(" raw_mean=");
    serialOut().print(rawWindowMean, 1);
    serialOut().print(" raw_abs_mean=");
    serialOut().print(rawWindowAbsMean, 1);
    serialOut().print(" raw_min=");
    serialOut().print(rawWindowMinOut);
    serialOut().print(" raw_max=");
    serialOut().print(rawWindowMaxOut);
    serialOut().print(" baseline=");
    serialOut().print(_audioSignal.baseline(), 1);
    serialOut().println();

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
