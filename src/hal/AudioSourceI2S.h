#pragma once

#include <cstddef>
#include <stdint.h>
#include <memory>

#include "../app/RuntimeDefaults.h"
#include "../audio/AudioSource.h"

/*
AudioSourceI2S

ESP32 I2S implementation of AudioSource.
Owns I2S setup, block reads, raw-sample diagnostics, and approximate block timing.
Does not know about AudioSignal, DetectionRuntime, Analyzer, or Behavior.
*/
class AudioSourceI2S : public AudioSource {
public:
    AudioSourceI2S(int sckPin = runtime::kDefaultAudioI2SSckPin,
                   int fsPin = runtime::kDefaultAudioI2SWsPin,
                   int dataInPin = runtime::kDefaultAudioI2SDataPin,
                   int sampleRate = static_cast<int>(runtime::kDefaultAudioI2SSampleRateHz),
                   int bitsPerSample = static_cast<int>(runtime::kDefaultAudioI2SBitsPerSample));

    void begin() override;
    bool available() override;
    int availableBytes() const override;
    bool readSample(int& sample, uint32_t& sampleTimeUs) override;
    bool readRawSample(int& sample, uint32_t& sampleTimeUs) override;
    bool readBlock(AudioBlock& block) override;
    uint32_t sampleRateHz() const override;
    const AudioSourceStats& stats() const override;
    void resetStats() override;

private:
    int32_t preprocessSample(int32_t current);
    void resetPreprocessState();
    bool refillBlock();
    void recordReadAttempt(int requestedBytes, int bytesRead, bool readError);

    int _sckPin;
    int _fsPin;
    int _dataInPin;
    int _sampleRate;
    int _bitsPerSample;
    runtime::PcmPreprocessMode _preprocessMode = runtime::kPcmPreprocessMode;
    bool _started = false;
    static constexpr size_t kRefillBatchSize = static_cast<size_t>(I2S_READ_BYTES / sizeof(int32_t));
    std::unique_ptr<int32_t[]> _blockSamples;
    size_t _blockCount = 0;
    size_t _blockCursor = 0;
    uint64_t _blockStartSampleIndex = 0;
    uint32_t _blockApproxStartMicros = 0;
    bool _blockOverflowBeforeBlock = false;
    uint64_t _outputSampleIndex = 0;
    int32_t _previousSample = 0;
    bool _hasPreviousSample = false;
    AudioSourceStats _stats;
};
