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
constexpr int32_t kRawCaptureFixedPointScale = 1000;

struct RawCaptureSample {
    uint16_t timeOffsetMs = 0;
    int32_t pcm = 0;
    int32_t amp = 0;
    int32_t targetScore = 0;
    uint8_t targetFresh : 1;
};

const char* rawCaptureModeName(AnalyzerApp::RawCaptureMode mode) {
    switch (mode) {
        case AnalyzerApp::RawCaptureMode::Features:
            return "feat";
        case AnalyzerApp::RawCaptureMode::Both:
            return "both";
        case AnalyzerApp::RawCaptureMode::Pcm:
        default:
            return "pcm";
    }
}

bool rawCaptureUsesFeatureStream(AnalyzerApp::RawCaptureMode mode) {
    return mode == AnalyzerApp::RawCaptureMode::Features || mode == AnalyzerApp::RawCaptureMode::Both;
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

    const uint32_t sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const unsigned long maxSamples = 8000UL;
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

    RawCaptureSample* preRingBuffer = nullptr;
    RawCaptureSample* captureBuffer = nullptr;

    if (preWantedSamples > 0) {
        preRingBuffer = static_cast<RawCaptureSample*>(malloc(static_cast<size_t>(preWantedSamples) * sizeof(RawCaptureSample)));
        if (preRingBuffer == nullptr) {
            Serial.println("RAW_ERR memory=pre_ring_alloc_failed");
            return false;
        }
    }

    captureBuffer = static_cast<RawCaptureSample*>(malloc(static_cast<size_t>(maxSamples) * sizeof(RawCaptureSample)));
    if (captureBuffer == nullptr) {
        if (preRingBuffer != nullptr) {
            free(preRingBuffer);
        }
        Serial.println("RAW_ERR memory=raw_buffer_alloc_failed");
        return false;
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
        free(captureBuffer);
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
    const bool useFeatureStream = rawCaptureUsesFeatureStream(mode);

    auto copyPreWindow = [&]() {
        if (preWindowCopied || preRingBuffer == nullptr || captureBuffer == nullptr || preWantedSamples == 0) {
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
            captureBuffer[i] = preRingBuffer[(startIndex + i) % preWantedSamples];
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
            if (mode != RawCaptureMode::Pcm) {
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
            if (mode != RawCaptureMode::Pcm) {
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

        const unsigned long sampleTimeMs = sampleTimeUs / 1000UL;
        const uint16_t sampleTimeOffsetMs = rawCaptureOffsetMs(sampleTimeMs, captureBaseMs);
        RawCaptureSample sample = {};
        sample.timeOffsetMs = sampleTimeOffsetMs;
        sample.pcm = static_cast<int32_t>(rawSample);

        if (useFeatureStream) {
            AudioSamplePacket audioSamplePacket = {};
            audioSamplePacket.sampleIndex = sampleTimeUs;
            audioSamplePacket.timeUs = sampleTimeUs;
            audioSamplePacket.timeMs = sampleTimeMs;
            audioSamplePacket.sampleRateHz = sampleRateHz;
            audioSamplePacket.rawAudioValue = rawSample;
            audioSamplePacket.baselineCorrectedValue = rawSample;
            audioSamplePacket.audioMagnitudeValue = static_cast<float>(rawSample < 0 ? -rawSample : rawSample);
            audioSamplePacket.level = rawSample < 0 ? -rawSample : rawSample;
            audioSamplePacket.smoothedLevel = audioSamplePacket.level;
            audioSamplePacket.baseline = 0.0f;
            audioSamplePacket.valid = true;
            audioSamplePacket.rawHistoryReady = true;

            _freqBandStream.observeCenteredSample(audioSamplePacket.baselineCorrectedValue, audioSamplePacket.timeMs);
            const detection::FrequencyBandMeasurementPacket frequencyEvidence = captureFrequencyMeasurementPacket(audioSamplePacket);
            _detection.observeFrame(audioSamplePacket, frequencyEvidence, audioSamplePacket.timeMs);

            const detection::FeatureHistory& featureHistory = _detection.featureHistory();
            sample.amp = rawCaptureToFixedPoint(featureHistory.latestValue(detection::FeatureStreamId::AmpEnvelope));
            sample.targetScore = rawCaptureToFixedPoint(featureHistory.latestValue(detection::FeatureStreamId::FrequencyTargetBand));
            sample.targetFresh = frequencyEvidence.fresh ? 1U : 0U;
        }

        if (!emitStarted) {
            if (preRingBuffer != nullptr) {
                preRingBuffer[preWriteIndex] = sample;
                preWriteIndex = (preWriteIndex + 1UL) % preWantedSamples;
            }
            ++preCapturedTotal;
        } else if (postCaptured < postWantedSamples) {
            if (captureBuffer != nullptr) {
                captureBuffer[preWindowSamples + postCaptured] = sample;
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

    const unsigned long capturedSamples = preWindowSamples + postCaptured;
    const unsigned long droppedSamples = (preWantedSamples + postWantedSamples) > capturedSamples
        ? (preWantedSamples + postWantedSamples) - capturedSamples
        : 0;

    float env = 0.0f;
    float maxEnv = 0.0f;
    int32_t maxAmpFixed = 0;
    int32_t maxTargetScoreFixed = 0;
    int32_t maxPcmAbs = 0;
    for (unsigned long i = 0; i < capturedSamples; ++i) {
        const RawCaptureSample& sample = captureBuffer[i];
        const int32_t absPcm = sample.pcm < 0 ? -sample.pcm : sample.pcm;
        if (absPcm > maxPcmAbs) {
            maxPcmAbs = absPcm;
        }
        if (sample.amp > maxAmpFixed) {
            maxAmpFixed = sample.amp;
        }
        if (sample.targetScore > maxTargetScoreFixed) {
            maxTargetScoreFixed = sample.targetScore;
        }
        if (useFeatureStream) {
            const float ampValue = static_cast<float>(sample.amp) / static_cast<float>(kRawCaptureFixedPointScale);
            env = env * 0.95f + ampValue * 0.05f;
            if (env > maxEnv) {
                maxEnv = env;
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
        Serial.print(" fields=ms,pcm");
    } else if (mode == RawCaptureMode::Features) {
        Serial.print(" fields=ms,amp,env,target_score,target_fresh");
    } else {
        Serial.print(" fields=ms,pcm,amp,env,target_score,target_fresh");
    }
    Serial.println();

    if (mode == RawCaptureMode::Pcm) {
        Serial.println("ms,pcm");
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const RawCaptureSample& sample = captureBuffer[i];
            Serial.print(static_cast<unsigned long>(sample.timeOffsetMs) + captureBaseMs);
            Serial.print(",");
            Serial.println(sample.pcm);
        }
    } else if (mode == RawCaptureMode::Features) {
        Serial.println("ms,amp,env,target_score,target_fresh");
        float csvEnv = 0.0f;
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const RawCaptureSample& sample = captureBuffer[i];
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
        Serial.println("ms,pcm,amp,env,target_score,target_fresh");
        float csvEnv = 0.0f;
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const RawCaptureSample& sample = captureBuffer[i];
            const float ampValue = static_cast<float>(sample.amp) / static_cast<float>(kRawCaptureFixedPointScale);
            const float targetScoreValue = static_cast<float>(sample.targetScore) / static_cast<float>(kRawCaptureFixedPointScale);
            csvEnv = csvEnv * 0.95f + ampValue * 0.05f;
            Serial.print(static_cast<unsigned long>(sample.timeOffsetMs) + captureBaseMs);
            Serial.print(",");
            Serial.print(sample.pcm);
            Serial.print(",");
            Serial.print(ampValue, 1);
            Serial.print(",");
            Serial.print(csvEnv, 1);
            Serial.print(",");
            Serial.print(targetScoreValue, 2);
            Serial.print(",");
            Serial.println(sample.targetFresh ? 1 : 0);
        }
    }

    Serial.print("RAW_SUMMARY id=");
    Serial.print(captureId);
    Serial.print(" captured=");
    Serial.print(capturedSamples);
    Serial.print(" dropped=");
    Serial.print(droppedSamples);
    Serial.print(" max_pcm_abs=");
    Serial.print(maxPcmAbs);
    if (useFeatureStream) {
        Serial.print(" max_amp=");
        Serial.print(static_cast<float>(maxAmpFixed) / static_cast<float>(kRawCaptureFixedPointScale), 1);
        Serial.print(" max_target_score=");
        Serial.print(static_cast<float>(maxTargetScoreFixed) / static_cast<float>(kRawCaptureFixedPointScale), 2);
        Serial.print(" max_env=");
        Serial.print(maxEnv, 1);
    }
    Serial.println();

    if (preRingBuffer != nullptr) {
        free(preRingBuffer);
    }
    free(captureBuffer);
    return true;
}
