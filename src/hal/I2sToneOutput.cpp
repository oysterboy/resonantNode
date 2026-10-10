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
      _amplitude(amplitude),
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
    const uint32_t phaseStep = static_cast<uint32_t>(
        (static_cast<uint64_t>(_toneHz) << 32) / static_cast<uint64_t>(sampleRateHz));
    const uint32_t rampSamples = (sampleRateHz * _rampMs) / 1000U;
    const float envelopeStep = rampSamples > 0 ? 1.0f / static_cast<float>(rampSamples) : 1.0f;
    const float target = _on ? 1.0f : 0.0f;

    for (size_t i = 0; i < frameCount; ++i) {
        if (_envelope < target) {
            _envelope = fminf(target, _envelope + envelopeStep);
        } else if (_envelope > target) {
            _envelope = fmaxf(target, _envelope - envelopeStep);
        }

        int32_t value = 0;
        if (_envelope > 0.0f) {
            const float s = sinf(static_cast<float>(_phase) * kPhaseToRadians);
            value = static_cast<int32_t>(s * _amplitude * _envelope * kFullScale);
            _phase += phaseStep;
        } else {
            // Restart each tone at phase 0 so every chirp has the same onset.
            _phase = 0;
        }
        frames[2 * i] = value;
        frames[2 * i + 1] = value;
    }
}
