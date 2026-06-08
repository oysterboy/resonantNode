#pragma once

#include <cstddef>
#include <stdint.h>
#include <memory>

#include "AudioSource.h"

struct AudioSlotDiagnostics {
    bool present = false;
    unsigned long slotCount[2] = {0, 0};
    int slotMin[2] = {0, 0};
    int slotMax[2] = {0, 0};
    double slotSumSquares[2] = {0.0, 0.0};
    unsigned long slotRepeatedRun[2] = {0, 0};
    const char* chosenSlot = "none";
    const char* activeSlot = "none";
};

/*
AudioSourceI2S

ESP32 I2S implementation of AudioSource.
Owns I2S setup, block reads, raw-sample diagnostics, and approximate block timing.
Does not know about AudioSignal, DetectionRuntime, Analyzer, or Behavior.
*/
class AudioSourceI2S : public AudioSource {
public:
    AudioSourceI2S(int sckPin, int fsPin, int dataInPin, int sampleRate = 16000, int bitsPerSample = 32);

    void begin() override;
    bool available() override;
    int availableBytes() const override;
    bool readSample(int& sample, uint32_t& sampleTimeUs) override;
    bool readRawSample(int& sample, uint32_t& sampleTimeUs) override;
    bool readBlock(AudioBlock& block) override;
    unsigned long droppedSamples() const override;
    unsigned long bufferedSamplesMax() const override;
    uint32_t sampleRateHz() const override;
    const AudioSourceStats& stats() const override;
    const AudioSlotDiagnostics& slotDiagnostics() const;
    void resetStats() override;
    uint32_t samplePeriodUs() const;

private:
    bool refillBlock();
    void recordReadAttempt(int requestedBytes, int bytesRead, bool readError);

    int _sckPin;
    int _fsPin;
    int _dataInPin;
    int _sampleRate;
    int _bitsPerSample;
    bool _started = false;
    static constexpr size_t kRefillBatchSize = 128;
    std::unique_ptr<int32_t[]> _blockSamples;
    size_t _blockCount = 0;
    size_t _blockCursor = 0;
    uint64_t _blockStartSampleIndex = 0;
    uint32_t _blockApproxStartMicros = 0;
    bool _blockOverflowBeforeBlock = false;
    unsigned long _droppedSamples = 0;
    size_t _maxBufferedSamples = 0;
    uint32_t _samplePeriodUs = 0;
    AudioSourceStats _stats;
    AudioSlotDiagnostics _slotDiagnostics;
};
