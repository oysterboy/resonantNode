#include "I2sToneOutput.h"

#include <Arduino.h>
#include <math.h>

namespace {
// One DMA buffer of stereo 32-bit frames per write: small enough that
// toneOn reaches the speaker within the TX queue depth plus one buffer.
constexpr size_t kBlockFrames = static_cast<size_t>(I2S_DMA_BUF_LEN);
constexpr uint32_t kWriteTimeoutMs = 50;
constexpr uint32_t kIdleRetryMs = 10;
constexpr uint32_t kTaskStackBytes = 4096;
constexpr UBaseType_t kTaskPriority = 5;
// Core 0: the Arduino loop (detection) runs on core 1.
constexpr BaseType_t kTaskCore = 0;
constexpr float kFullScale = 2147483647.0f;
constexpr float kTwoPi = 6.28318530718f;
constexpr float kPhaseToRadians = kTwoPi / 4294967296.0f;
}

I2sToneOutput::I2sToneOutput(AudioSourceI2S& port, float amplitude, uint32_t rampMs)
    : _port(port),
      // Below 1.0: at full amplitude s * kFullScale reaches 2^31, outside
      // int32 (issue #31). The default level is 0.3.
      _amplitude(amplitude < 0.0f ? 0.0f : (amplitude > 0.999f ? 0.999f : amplitude)),
      _rampMs(rampMs) {}

void I2sToneOutput::begin() {
    _on = false;
    if (_task != nullptr) {
        return;
    }
    _stopRequested = false;
    TaskHandle_t task = nullptr;
    xTaskCreatePinnedToCore(&I2sToneOutput::taskEntry, "i2s_tone", kTaskStackBytes, this,
                            kTaskPriority, &task, kTaskCore);
    _task = task;
}

void I2sToneOutput::end() {
    _on = false;
    if (_task == nullptr) {
        return;
    }
    // Let the ramp reach zero, then ask the task to leave after its next
    // block (one block is 8 ms; a write waits at most kWriteTimeoutMs).
    delay(_rampMs + 20);
    _stopRequested = true;
    for (int i = 0; i < 40 && _task != nullptr; ++i) {
        delay(5);
    }
}

void I2sToneOutput::setToneHz(uint32_t toneHz) {
    _toneHz = toneHz;
}

void I2sToneOutput::toneOn() {
    _on = true;
}

void I2sToneOutput::toneOff() {
    _on = false;
    portENTER_CRITICAL(&_trainMux);
    _trainRequested = false;
    _trainCancelRequested = true;
    _trainActive = false;
    portEXIT_CRITICAL(&_trainMux);
}

void I2sToneOutput::setPulseHop(const uint32_t* hz, size_t n) {
    portENTER_CRITICAL(&_trainMux);
    _reqHopCount = (hz == nullptr || n > kMaxHop) ? 0 : n;
    for (size_t i = 0; i < _reqHopCount; ++i) {
        _reqHopHz[i] = hz[i];
    }
    portEXIT_CRITICAL(&_trainMux);
}

bool I2sToneOutput::startPulseTrain(uint32_t count, uint32_t onMs, uint32_t gapMs, uint32_t rampMs) {
    if (_task == nullptr || count == 0 || onMs == 0) {
        return false;
    }
    _on = false;
    portENTER_CRITICAL(&_trainMux);
    _reqTrainCount = count;
    _reqTrainOnMs = onMs;
    _reqTrainGapMs = gapMs;
    _reqTrainRampMs = rampMs;
    // A new train replaces a running one, so a pending cancel is moot.
    _trainCancelRequested = false;
    _trainRequested = true;
    _trainActive = true;
    portEXIT_CRITICAL(&_trainMux);
    return true;
}

bool I2sToneOutput::pulseTrainActive() const {
    portENTER_CRITICAL(&_trainMux);
    const bool active = _trainActive;
    portEXIT_CRITICAL(&_trainMux);
    return active;
}

void I2sToneOutput::takePulseTrainRequest(uint32_t sampleRateHz) {
    portENTER_CRITICAL(&_trainMux);
    if (_trainCancelRequested) {
        _trainCancelRequested = false;
        _trainRunning = false;
        _trainDraining = false;
        _trainStep = 0;
    }
    if (_trainRequested) {
        _trainRequested = false;
        _trainRunning = true;
        _trainDraining = false;
        _trainOn = true;
        _trainPulsesLeft = _reqTrainCount;
        _hopCount = _reqHopCount;
        for (size_t i = 0; i < _hopCount; ++i) {
            _hopHz[i] = _reqHopHz[i];
        }
        _hopIndex = 0;
        _trainStep = 0;
        if (_hopCount > 0) {
            _trainStep = hopStep(sampleRateHz);
        }
        // onMs/gapMs are what is heard: sound (envelope > 0) and silence.
        // The off-ramp runs past the "on" target, so the target is onMs -
        // ramp and the gap target gapMs + ramp; a ramp over half the pulse
        // is cut to half (the pulse is then a triangle).
        const uint32_t pulseSamples = (sampleRateHz * _reqTrainOnMs) / 1000U;
        uint32_t rampMs = _reqTrainRampMs;
        if (rampMs > _reqTrainOnMs / 2) {
            rampMs = _reqTrainOnMs / 2;
        }
        const uint32_t rampSamples = (sampleRateHz * rampMs) / 1000U;
        _trainOnSamples = pulseSamples > rampSamples ? pulseSamples - rampSamples : 1;
        _trainGapSamples = (sampleRateHz * _reqTrainGapMs) / 1000U + rampSamples;
        _trainRampMs = rampMs;
        _trainSegmentLeft = _trainOnSamples;
    }
    portEXIT_CRITICAL(&_trainMux);
}

// Phase step of the next hop entry (cycled), advancing the index.
uint32_t I2sToneOutput::hopStep(uint32_t sampleRateHz) {
    const uint32_t hz = _hopHz[_hopIndex % _hopCount];
    ++_hopIndex;
    return static_cast<uint32_t>((static_cast<uint64_t>(hz) << 32) / static_cast<uint64_t>(sampleRateHz));
}

void I2sToneOutput::taskEntry(void* self) {
    static_cast<I2sToneOutput*>(self)->run();
}

void I2sToneOutput::run() {
    int32_t frames[kBlockFrames * 2];
    for (;;) {
        if (_stopRequested) {
            _stopRequested = false;
            _phase = 0;
            _envelope = 0.0f;
            _trainRunning = false;
            _trainDraining = false;
            portENTER_CRITICAL(&_trainMux);
            _trainRequested = false;
            _trainActive = false;
            portEXIT_CRITICAL(&_trainMux);
            _task = nullptr;
            vTaskDelete(nullptr);
        }
        renderBlock(frames, kBlockFrames);
        size_t written = 0;
        if (!_port.writeTx(frames, sizeof(frames), written, kWriteTimeoutMs)) {
            // Port not installed (yet), being reinstalled, or no TX on this
            // board: drop the block and retry shortly.
            vTaskDelay(pdMS_TO_TICKS(kIdleRetryMs));
        }
    }
}

void I2sToneOutput::renderBlock(int32_t* frames, size_t frameCount) {
    const uint32_t sampleRateHz = _port.sampleRateHz() > 0 ? _port.sampleRateHz() : 16000U;
    takePulseTrainRequest(sampleRateHz);
    const uint32_t phaseStep = static_cast<uint32_t>(
        (static_cast<uint64_t>(_toneHz) << 32) / static_cast<uint64_t>(sampleRateHz));
    const bool trainShape = _trainRunning || _trainDraining;
    const uint32_t rampSamples = (sampleRateHz * (trainShape ? _trainRampMs : _rampMs)) / 1000U;
    const float envelopeStep = rampSamples > 0 ? 1.0f / static_cast<float>(rampSamples) : 1.0f;
    float target = _on ? 1.0f : 0.0f;

    for (size_t i = 0; i < frameCount; ++i) {
        if (_trainRunning) {
            if (_trainSegmentLeft == 0) {
                if (_trainOn) {
                    --_trainPulsesLeft;
                    _trainOn = false;
                    _trainSegmentLeft = _trainGapSamples;
                    if (_trainPulsesLeft == 0) {
                        _trainRunning = false;
                        _trainDraining = true;
                    }
                } else {
                    _trainOn = true;
                    _trainSegmentLeft = _trainOnSamples;
                    if (_hopCount > 0) {
                        _trainStep = hopStep(sampleRateHz);
                    }
                }
            }
            if (_trainRunning && _trainSegmentLeft > 0) {
                --_trainSegmentLeft;
            }
            target = (_trainRunning && _trainOn) ? 1.0f : 0.0f;
        } else if (_trainDraining) {
            target = 0.0f;
            if (_envelope <= 0.0f) {
                _trainDraining = false;
                _trainStep = 0;
                portENTER_CRITICAL(&_trainMux);
                if (!_trainRequested) {
                    _trainActive = false;
                }
                portEXIT_CRITICAL(&_trainMux);
                target = _on ? 1.0f : 0.0f;
            }
        }

        if (_envelope < target) {
            _envelope = fminf(target, _envelope + envelopeStep);
        } else if (_envelope > target) {
            _envelope = fmaxf(target, _envelope - envelopeStep);
        }

        int32_t value = 0;
        if (_envelope > 0.0f) {
            const float s = sinf(static_cast<float>(_phase) * kPhaseToRadians);
            value = static_cast<int32_t>(s * _amplitude * _envelope * kFullScale);
            _phase += (trainShape && _trainStep != 0) ? _trainStep : phaseStep;
        } else {
            // Restart each tone at phase 0 so every chirp has the same onset.
            _phase = 0;
        }
        frames[2 * i] = value;
        frames[2 * i + 1] = value;
    }
}
