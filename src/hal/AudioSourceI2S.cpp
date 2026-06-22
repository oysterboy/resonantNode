#include "AudioSourceI2S.h"

#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>
#include <new>
#include <string.h>

#include "../audio/AudioPcm.h"

namespace {
constexpr i2s_port_t kI2sPort = I2S_NUM_0;

int decodePcmSample(const uint8_t* samplePtr, int bytesPerSample) {
    if (samplePtr == nullptr || bytesPerSample <= 0) {
        return 0;
    }

    if (bytesPerSample >= static_cast<int>(sizeof(int32_t))) {
        int32_t sample32 = 0;
        memcpy(&sample32, samplePtr, sizeof(sample32));
        return static_cast<int>(audio::clampToCanonicalPcm(static_cast<audio::PcmIntermediate>(sample32) >> 8));
    }

    if (bytesPerSample == 3) {
        uint32_t packed = 0;
        memcpy(&packed, samplePtr, 3);
        if ((packed & 0x00800000UL) != 0) {
            packed |= 0xFF000000UL;
        }
        const int32_t signedPacked = static_cast<int32_t>(packed);
        return static_cast<int>(audio::clampToCanonicalPcm(static_cast<audio::PcmIntermediate>(signedPacked)));
    }

    if (bytesPerSample == 2) {
        int16_t sample16 = 0;
        memcpy(&sample16, samplePtr, sizeof(sample16));
        return static_cast<int>(sample16);
    }

    int8_t sample8 = 0;
    memcpy(&sample8, samplePtr, sizeof(sample8));
    return static_cast<int>(sample8);
}

uint32_t sampleOffsetUs(uint32_t sampleOffset, uint32_t sampleRateHz) {
    if (sampleRateHz == 0) {
        return 0;
    }

    return static_cast<uint32_t>((static_cast<uint64_t>(sampleOffset) * 1000000ULL) / static_cast<uint64_t>(sampleRateHz));
}

} // namespace

AudioSourceI2S::AudioSourceI2S(int sckPin, int fsPin, int dataInPin, int sampleRate, int bitsPerSample)
    : _sckPin(sckPin),
      _fsPin(fsPin),
      _dataInPin(dataInPin),
      _sampleRate(sampleRate),
      _bitsPerSample(bitsPerSample),
      _preprocessMode(runtime::kPcmPreprocessMode),
      _blockSamples(new (std::nothrow) int32_t[kRefillBatchSize] {}) {}

void AudioSourceI2S::begin() {
    resetStats();
    resetPreprocessState();
    if (!_blockSamples) {
        _blockSamples.reset(new (std::nothrow) int32_t[kRefillBatchSize] {});
    }

    _started = false;
    (void)i2s_driver_uninstall(kI2sPort);

    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_CAPTURE_MODE);
    config.sample_rate = static_cast<uint32_t>(_sampleRate);
    config.bits_per_sample = static_cast<i2s_bits_per_sample_t>(_bitsPerSample);
    config.channel_format = static_cast<i2s_channel_fmt_t>(I2S_CHANNEL_FORMAT_VALUE);
    config.communication_format = static_cast<i2s_comm_format_t>(I2S_COMM_FORMAT_VALUE);
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL2;
    config.dma_buf_count = I2S_DMA_BUF_COUNT;
    config.dma_buf_len = I2S_DMA_BUF_LEN;
    config.use_apll = I2S_USE_APLL != 0;
    config.tx_desc_auto_clear = false;
    config.fixed_mclk = config.use_apll ? 512 * _sampleRate : 0;
    config.mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT;
    config.bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT;

    if (i2s_driver_install(kI2sPort, &config, 0, nullptr) != ESP_OK) {
        return;
    }

    i2s_pin_config_t pinConfig = {};
    pinConfig.mck_io_num = I2S_PIN_NO_CHANGE;
    pinConfig.bck_io_num = _sckPin;
    pinConfig.ws_io_num = _fsPin;
    pinConfig.data_out_num = I2S_PIN_NO_CHANGE;
    pinConfig.data_in_num = _dataInPin;
    if (i2s_set_pin(kI2sPort, &pinConfig) != ESP_OK) {
        (void)i2s_driver_uninstall(kI2sPort);
        return;
    }

    if (i2s_set_clk(kI2sPort, static_cast<uint32_t>(_sampleRate), static_cast<uint32_t>(_bitsPerSample), I2S_CHANNEL_MONO) != ESP_OK) {
        (void)i2s_driver_uninstall(kI2sPort);
        return;
    }

    (void)i2s_zero_dma_buffer(kI2sPort);
    _started = true;
}

bool AudioSourceI2S::available() {
    return _blockCursor < _blockCount || refillBlock();
}

int AudioSourceI2S::availableBytes() const {
    if (_blockCursor >= _blockCount) {
        return 0;
    }

    const size_t bytesPerSample = static_cast<size_t>(_bitsPerSample / 8);
    return static_cast<int>((_blockCount - _blockCursor) * bytesPerSample);
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

    uint8_t rawBytes[sizeof(int32_t)] = {};
    size_t bytesRead = 0;
    const esp_err_t readResult = i2s_read(kI2sPort, rawBytes, static_cast<size_t>(bytesPerSample), &bytesRead, 0);
    recordReadAttempt(bytesPerSample, static_cast<int>(bytesRead), readResult != ESP_OK && readResult != ESP_ERR_TIMEOUT);
    if (bytesRead < static_cast<size_t>(bytesPerSample)) {
        return false;
    }

    sample = decodePcmSample(rawBytes, bytesPerSample);
    sampleTimeUs = micros();
    _stats.totalSamplesRead += 1;
    return true;
}

bool AudioSourceI2S::readBlock(AudioBlock& block) {
    if (!_blockSamples) {
        block = {};
        return false;
    }

    if (_blockCursor >= _blockCount && !refillBlock()) {
        block = {};
        return false;
    }

    if (_blockCursor >= _blockCount) {
        block = {};
        return false;
    }

    block.samples = _blockSamples.get() + _blockCursor;
    block.sampleCount = static_cast<uint16_t>(_blockCount - _blockCursor);
    block.startSampleIndex = _blockStartSampleIndex + _blockCursor;
    block.approxStartMicros = _blockApproxStartMicros + sampleOffsetUs(static_cast<uint32_t>(_blockCursor), static_cast<uint32_t>(_sampleRate));
    block.overflowBeforeBlock = false;
    _blockCursor = _blockCount;
    return true;
}

uint32_t AudioSourceI2S::sampleRateHz() const {
    return static_cast<uint32_t>(_sampleRate);
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
    _outputSampleIndex = 0;
    _stats = {};
}

int32_t AudioSourceI2S::preprocessSample(int32_t current) {
    if (_preprocessMode == runtime::PcmPreprocessMode::None) {
        return current;
    }

    if (!_hasPreviousSample) {
        _previousSample = current;
        _hasPreviousSample = true;
        return 0;
    }

    const int64_t diff = static_cast<int64_t>(current) - static_cast<int64_t>(_previousSample);
    _previousSample = current;
    return audio::clampToCanonicalPcm(diff / 2);
}

void AudioSourceI2S::resetPreprocessState() {
    _previousSample = 0;
    _hasPreviousSample = false;
}

void AudioSourceI2S::recordReadAttempt(int requestedBytes, int bytesRead, bool readError) {
    ++_stats.reads;
    if (bytesRead < 0) {
        bytesRead = 0;
    }

    _stats.readBytes += static_cast<uint32_t>(bytesRead);
    if (static_cast<uint32_t>(bytesRead) > _stats.maxReadBytes) {
        _stats.maxReadBytes = static_cast<uint32_t>(bytesRead);
    }

    if (bytesRead == 0) {
        ++_stats.zeroReads;
        ++_stats.noSampleLoops;
    } else if (requestedBytes > 0 && bytesRead < requestedBytes) {
        ++_stats.shortReads;
    }

    if (readError) {
        ++_stats.readErrors;
    }
}

bool AudioSourceI2S::refillBlock() {
    if (!_blockSamples || !_started) {
        recordReadAttempt(0, 0, true);
        return false;
    }

    const int bytesPerSample = _bitsPerSample / 8;
    if (bytesPerSample <= 0) {
        recordReadAttempt(0, 0, true);
        return false;
    }

    uint8_t rawBytes[kRefillBatchSize * sizeof(int32_t)] = {};
    const size_t requestedBytes = static_cast<size_t>(kRefillBatchSize) * static_cast<size_t>(bytesPerSample);
    size_t bytesRead = 0;
    const esp_err_t readResult = i2s_read(kI2sPort, rawBytes, requestedBytes, &bytesRead, 0);
    recordReadAttempt(static_cast<int>(requestedBytes), static_cast<int>(bytesRead), readResult != ESP_OK && readResult != ESP_ERR_TIMEOUT);
    if (bytesRead == 0) {
        return false;
    }

    const size_t fullSamplesRead = bytesRead / static_cast<size_t>(bytesPerSample);
    const size_t samplesToProcess = fullSamplesRead < kRefillBatchSize ? fullSamplesRead : kRefillBatchSize;
    const uint32_t fillEndUs = micros();

    _blockCursor = 0;
    _blockStartSampleIndex = _outputSampleIndex;
    _blockCount = 0;
    for (size_t i = 0; i < samplesToProcess; ++i) {
        const uint8_t* samplePtr = rawBytes + (i * static_cast<size_t>(bytesPerSample));
        const int32_t decodedSample = static_cast<int32_t>(decodePcmSample(samplePtr, bytesPerSample));
        _blockSamples[_blockCount++] = preprocessSample(decodedSample);
    }

    const uint32_t selectedFrameAge = _blockCount > 0 ? static_cast<uint32_t>(_blockCount - 1U) : 0U;
    const uint32_t offsetUs = sampleOffsetUs(selectedFrameAge, static_cast<uint32_t>(_sampleRate));
    _blockApproxStartMicros = fillEndUs > offsetUs ? fillEndUs - offsetUs : 0U;
    _outputSampleIndex += static_cast<uint64_t>(_blockCount);
    _stats.totalSamplesRead += static_cast<uint64_t>(_blockCount);
    return _blockCount > 0;
}
