#include "AudioSourceI2S.h"
#include <I2S.h>
#include <string.h>
#include <new>

namespace {
int normalizeToAdcScale(int32_t rawSample) {
    // The analog path feeds 12-bit ADC-style values around 0..4095.
    // Map the signed I2S sample into the same range so the rest of the
    // pipeline can reuse the analog tuning unchanged.
    const int32_t signed12 = rawSample >> 20;
    const int32_t clamped = signed12 < -2048 ? -2048 : (signed12 > 2047 ? 2047 : signed12);
    return static_cast<int>(clamped + 2048);
}

uint32_t sampleOffsetUs(uint32_t sampleOffset, uint32_t sampleRateHz) {
    if (sampleRateHz == 0) {
        return 0;
    }

    return static_cast<uint32_t>((static_cast<uint64_t>(sampleOffset) * 1000000ULL) / static_cast<uint64_t>(sampleRateHz));
}
}

AudioSourceI2S::AudioSourceI2S(int sckPin, int fsPin, int dataInPin, int sampleRate, int bitsPerSample)
    : _sckPin(sckPin),
      _fsPin(fsPin),
      _dataInPin(dataInPin),
      _sampleRate(sampleRate),
      _bitsPerSample(bitsPerSample),
      _blockSamples(new (std::nothrow) int32_t[kRefillBatchSize] {}) {}

void AudioSourceI2S::begin() {
    // Keep the public contract sample-based even though I2S may buffer internally.
    resetStats();
    if (!_blockSamples) {
        _blockSamples.reset(new (std::nothrow) int32_t[kRefillBatchSize] {});
    }
    _started = false;
    _samplePeriodUs = _sampleRate > 0 ? static_cast<uint32_t>(1000000UL / static_cast<uint32_t>(_sampleRate)) : 0;

    I2S.end();
    I2S.setAllPins(_sckPin, _fsPin, _dataInPin, I2S_PIN_NO_CHANGE, I2S_PIN_NO_CHANGE);
    const int beginResult = I2S.begin(I2S_PHILIPS_MODE, _sampleRate, _bitsPerSample);
    const bool beginSucceeded = beginResult != 0;
    if (beginSucceeded) {
        // Force mono at the source so the RX stream does not expose the inactive slot as alternating zeros.
        const esp_err_t monoResult = esp_i2s::i2s_set_clk(
            static_cast<esp_i2s::i2s_port_t>(0),
            static_cast<uint32_t>(_sampleRate),
            static_cast<esp_i2s::i2s_bits_per_sample_t>(_bitsPerSample),
            esp_i2s::I2S_CHANNEL_MONO);
        _started = monoResult == ESP_OK;
    } else {
        _started = false;
    }
}

bool AudioSourceI2S::available() {
    return _blockCursor < _blockCount || refillBlock();
}

int AudioSourceI2S::availableBytes() const {
    return I2S.available();
}

bool AudioSourceI2S::readSample(int& sample, uint32_t& sampleTimeUs) {
    if (_blockCursor >= _blockCount && !refillBlock()) {
        return false;
    }

    if (_blockCursor >= _blockCount) {
        return false;
    }

    const size_t index = _blockCursor++;
    sample = static_cast<int>(_blockSamples[index]);
    sampleTimeUs = _blockApproxStartMicros + sampleOffsetUs(static_cast<uint32_t>(index), static_cast<uint32_t>(_sampleRate));
    return true;
}

bool AudioSourceI2S::readRawSample(int& sample, uint32_t& sampleTimeUs) {
    if (!_started) {
        recordReadAttempt(0, 0, true);
        return false;
    }

    const int bytesPerSample = _bitsPerSample / 8;
    if (bytesPerSample <= 0) {
        recordReadAttempt(0, 0, true);
        return false;
    }

    const int availableBytes = I2S.available();
    if (availableBytes < bytesPerSample) {
        recordReadAttempt(0, 0, false);
        return false;
    }

    uint8_t rawBytes[sizeof(int32_t)] = {};
    const int bytesRead = I2S.read(rawBytes, static_cast<size_t>(bytesPerSample));
    recordReadAttempt(bytesPerSample, bytesRead, bytesRead <= 0);
    if (bytesRead < bytesPerSample) {
        return false;
    }

    int32_t rawSample = 0;
    memcpy(&rawSample, rawBytes, static_cast<size_t>(bytesPerSample));
    sample = static_cast<int>(rawSample);
    sampleTimeUs = micros();
    _stats.totalSamplesRead += 1;
    return true;
}

bool AudioSourceI2S::readBlock(AudioBlock& block) {
    if (!_blockSamples) {
        block.samples = nullptr;
        block.sampleCount = 0;
        return false;
    }

    if (_blockCursor >= _blockCount && !refillBlock()) {
        return false;
    }

    if (_blockCursor >= _blockCount) {
        return false;
    }

    block.samples = _blockSamples.get() + _blockCursor;
    block.sampleCount = static_cast<uint16_t>(_blockCount - _blockCursor);
    block.startSampleIndex = _blockStartSampleIndex + _blockCursor;
    block.approxStartMicros = _blockApproxStartMicros + sampleOffsetUs(static_cast<uint32_t>(_blockCursor), static_cast<uint32_t>(_sampleRate));
    block.overflowBeforeBlock = _blockOverflowBeforeBlock;
    _blockCursor = _blockCount;
    return true;
}

unsigned long AudioSourceI2S::droppedSamples() const {
    return _droppedSamples;
}

unsigned long AudioSourceI2S::bufferedSamplesMax() const {
    return static_cast<unsigned long>(_maxBufferedSamples);
}

uint32_t AudioSourceI2S::sampleRateHz() const {
    return static_cast<uint32_t>(_sampleRate);
}

uint32_t AudioSourceI2S::samplePeriodUs() const {
    return _samplePeriodUs;
}

const AudioSourceStats& AudioSourceI2S::stats() const {
    return _stats;
}

void AudioSourceI2S::resetStats() {
    _blockCursor = 0;
    _blockCount = 0;
    _blockStartSampleIndex = 0;
    _blockApproxStartMicros = 0;
    _blockOverflowBeforeBlock = false;
    _droppedSamples = 0;
    _maxBufferedSamples = 0;
    _stats = {};
}

void AudioSourceI2S::recordReadAttempt(int requestedBytes, int bytesRead, bool readError) {
    _stats.reads++;
    if (bytesRead < 0) {
        bytesRead = 0;
    }

    _stats.readBytes += static_cast<uint32_t>(bytesRead);
    if (static_cast<uint32_t>(bytesRead) > _stats.maxReadBytes) {
        _stats.maxReadBytes = static_cast<uint32_t>(bytesRead);
    }

    if (bytesRead == 0) {
        _stats.zeroReads++;
        _stats.noSampleLoops++;
    } else {
        if (requestedBytes > 0 && bytesRead < requestedBytes) {
            _stats.shortReads++;
        }
    }

    if (readError) {
        _stats.readErrors++;
    }
}

bool AudioSourceI2S::refillBlock() {
    if (!_blockSamples) {
        recordReadAttempt(0, 0, true);
        return false;
    }

    if (!_started) {
        recordReadAttempt(0, 0, true);
        return false;
    }

    const int bytesPerSample = _bitsPerSample / 8;
    if (bytesPerSample <= 0) {
        recordReadAttempt(0, 0, true);
        return false;
    }

    const int availableBytes = I2S.available();
    if (availableBytes <= 0) {
        recordReadAttempt(0, 0, false);
        return false;
    }

    int requestedBytes = availableBytes;
    const int maxBatchBytes = static_cast<int>(kRefillBatchSize) * bytesPerSample;
    if (requestedBytes > maxBatchBytes) {
        requestedBytes = maxBatchBytes;
    }
    requestedBytes -= requestedBytes % bytesPerSample;
    if (requestedBytes <= 0) {
        recordReadAttempt(0, 0, false);
        return false;
    }

    uint8_t rawBytes[kRefillBatchSize * sizeof(int32_t)] = {};
    const int bytesRead = I2S.read(rawBytes, static_cast<size_t>(requestedBytes));
    recordReadAttempt(requestedBytes, bytesRead, bytesRead <= 0);
    if (bytesRead <= 0) {
        return false;
    }

    const size_t fullSamplesRead = static_cast<size_t>(bytesRead) / static_cast<size_t>(bytesPerSample);
    const uint32_t fillEndUs = micros();
    const uint32_t availableSamples = static_cast<uint32_t>(availableBytes / bytesPerSample);
    const uint32_t readSamples = static_cast<uint32_t>(fullSamplesRead);
    const uint32_t firstReturnedSampleAgeSamples = availableSamples > 0
        ? availableSamples - 1U
        : (readSamples > 0 ? readSamples - 1U : 0U);
    const uint32_t firstReturnedSampleOffsetUs = sampleOffsetUs(firstReturnedSampleAgeSamples, static_cast<uint32_t>(_sampleRate));
    const size_t sampleBytes = static_cast<size_t>(bytesPerSample);
    const size_t samplesToProcess = fullSamplesRead;
    _blockCount = 0;
    _blockCursor = 0;
    _blockStartSampleIndex = _stats.totalSamplesRead;
    _blockApproxStartMicros = fillEndUs > firstReturnedSampleOffsetUs ? fillEndUs - firstReturnedSampleOffsetUs : 0U;
    _blockOverflowBeforeBlock = _droppedSamples > 0;
    for (size_t i = 0; i < samplesToProcess; ++i) {
        int rawSample = 0;
        const uint8_t* samplePtr = rawBytes + (i * sampleBytes);
        if (bytesPerSample == 4) {
            int32_t sample32 = 0;
            memcpy(&sample32, samplePtr, sizeof(sample32));
            rawSample = sample32;
        } else if (bytesPerSample == 2) {
            int16_t sample16 = 0;
            memcpy(&sample16, samplePtr, sizeof(sample16));
            rawSample = sample16;
        } else if (bytesPerSample == 1) {
            int8_t sample8 = 0;
            memcpy(&sample8, samplePtr, sizeof(sample8));
            rawSample = sample8;
        } else {
            memcpy(&rawSample, samplePtr, sampleBytes);
        }

        const int sample = normalizeToAdcScale(rawSample);
        if (i < kRefillBatchSize) {
            _blockSamples[i] = sample;
            _blockCount++;
            if (_blockCount > _maxBufferedSamples) {
                _maxBufferedSamples = _blockCount;
            }
        } else {
            ++_droppedSamples;
            _stats.overflowCount++;
        }
    }
    _stats.totalSamplesRead += static_cast<uint64_t>(fullSamplesRead);
    return _blockCount > 0;
}
