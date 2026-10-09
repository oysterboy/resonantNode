#include "AudioSourceI2S.h"

#include <Arduino.h>
#include <driver/i2s.h>
#include <soc/i2s_struct.h>
#include <math.h>
#include <new>
#include <stdint.h>
#include <string.h>

#include "../audio/AudioPcm.h"

namespace {
constexpr i2s_port_t kI2sPort = I2S_NUM_0;
constexpr bool kPortHasTx = ((I2S_CAPTURE_MODE) & I2S_MODE_TX) != 0;

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

AudioSourceI2S::AudioSourceI2S(int sckPin, int fsPin, int dataInPin, int sampleRate, int bitsPerSample, int dataOutPin)
    : _sckPin(sckPin),
      _fsPin(fsPin),
      _dataInPin(dataInPin),
      _dataOutPin(dataOutPin),
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

    // A TX writer (I2sToneOutput's task) may be inside i2s_write(); keep it
    // out while the driver is torn down and reinstalled.
    if (_portMutex == nullptr) {
        _portMutex = xSemaphoreCreateMutex();
    }
    if (_portMutex != nullptr) {
        xSemaphoreTake(_portMutex, portMAX_DELAY);
    }
    _started = installPort();
    if (_portMutex != nullptr) {
        xSemaphoreGive(_portMutex);
    }
}

bool AudioSourceI2S::installPort() {
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
    // TX underrun sends silence instead of repeating the last buffer.
    config.tx_desc_auto_clear = kPortHasTx;
    config.fixed_mclk = config.use_apll ? 512 * _sampleRate : 0;
    config.mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT;
    config.bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT;

    // One second of events (RX_DONE per buffer, plus TX_DONE on a
    // full-duplex port) so a long loop stall does not overflow the event
    // queue and hide an RX_Q_OVF.
    _i2sEvents = nullptr;
#ifndef I2S_EVENT_QUEUE_DISABLED
    const int eventQueueLength = (kPortHasTx ? 2 : 1) * (_sampleRate / I2S_DMA_BUF_LEN) + 4;
    if (i2s_driver_install(kI2sPort, &config, eventQueueLength, &_i2sEvents) != ESP_OK) {
#else
    if (i2s_driver_install(kI2sPort, &config, 0, nullptr) != ESP_OK) {
#endif
        return false;
    }

    i2s_pin_config_t pinConfig = {};
    pinConfig.mck_io_num = I2S_PIN_NO_CHANGE;
    pinConfig.bck_io_num = _sckPin;
    pinConfig.ws_io_num = _fsPin;
    pinConfig.data_out_num = (kPortHasTx && _dataOutPin >= 0) ? _dataOutPin : I2S_PIN_NO_CHANGE;
    pinConfig.data_in_num = _dataInPin;
    if (i2s_set_pin(kI2sPort, &pinConfig) != ESP_OK) {
        (void)i2s_driver_uninstall(kI2sPort);
        return false;
    }

    const i2s_channel_t channels = kFrameSlots >= 2 ? I2S_CHANNEL_STEREO : I2S_CHANNEL_MONO;
    if (i2s_set_clk(kI2sPort, static_cast<uint32_t>(_sampleRate), static_cast<uint32_t>(_bitsPerSample), channels) != ESP_OK) {
        (void)i2s_driver_uninstall(kI2sPort);
        return false;
    }

#if I2S_RX_MSB_ALIGN
    // STAND_I2S (Philips) is what the MAX98357A expects on TX, but on this
    // IDF 4.4 build it reads the INMP441 one bit late (bit 8 always 0,
    // values doubled, sign bit lost; issue #24). The legacy driver sets RX
    // and TX framing together, so clear the RX MSB shift alone: RX then
    // gets the STAND_MSB alignment, TX stays Philips.
    // docs/decisions/2026-10-09-damp-i2s-hal-shape.md
    (void)i2s_stop(kI2sPort);
    I2S0.conf.rx_msb_shift = 0;
    (void)i2s_start(kI2sPort);
#endif

    (void)i2s_zero_dma_buffer(kI2sPort);
    return true;
}

bool AudioSourceI2S::txEnabled() const {
    return kPortHasTx && _dataOutPin >= 0;
}

bool AudioSourceI2S::writeTx(const void* data, size_t bytes, size_t& bytesWritten, uint32_t timeoutMs) {
    bytesWritten = 0;
    if (!txEnabled() || _portMutex == nullptr) {
        return false;
    }
    const TickType_t timeoutTicks = pdMS_TO_TICKS(timeoutMs);
    if (xSemaphoreTake(_portMutex, timeoutTicks) != pdTRUE) {
        return false;
    }
    bool ok = false;
    if (_started) {
        ok = i2s_write(kI2sPort, data, bytes, &bytesWritten, timeoutTicks) == ESP_OK;
    }
    xSemaphoreGive(_portMutex);
    return ok;
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

    uint8_t frameBytes[kFrameSlots * sizeof(int32_t)] = {};
    const size_t frameSize = kFrameSlots * static_cast<size_t>(bytesPerSample);
    size_t bytesRead = 0;
    const esp_err_t readResult = i2s_read(kI2sPort, frameBytes, frameSize, &bytesRead, 0);
    recordReadAttempt(static_cast<int>(frameSize), static_cast<int>(bytesRead), readResult != ESP_OK && readResult != ESP_ERR_TIMEOUT);
    if (bytesRead < frameSize) {
        return false;
    }
    const uint8_t* rawBytes = frameBytes + kMicSlot * static_cast<size_t>(bytesPerSample);
    // Keep the First Difference state current, so the next block does not
    // difference across the RAW gap and emit one spurious spike.
    (void)preprocessSample(static_cast<int32_t>(decodePcmSample(rawBytes, bytesPerSample)));

#ifdef RAW_I2S_UNDECODED
    // Diagnostic build only (issue #24): hand RAW mode=i2s the undecoded
    // 32-bit slot word so framing (the INMP441's unused low byte) can be
    // checked offline.
    int32_t undecoded = 0;
    memcpy(&undecoded, rawBytes, sizeof(undecoded));
    sample = static_cast<int>(undecoded);
#else
    sample = decodePcmSample(rawBytes, bytesPerSample);
#endif
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

// Block timestamps come from the sample index on a clock locked to
// micros(), not from the time a block happens to be read.
//
// i2s_read() hands over whole DMA buffers (128 samples, 8 ms) that may have
// been waiting for a while. Stamping each block as "its last sample arrived
// now" turned every late read into a jump forward and every catch-up read
// into a jump back, so the sample time base had holes and overlaps whenever
// the loop ran behind. FeatureHistory bins by that time, and an inspection
// window with a hole at its edge was reported HistoryWindowIncomplete
// (issue #26).
//
// The clock: time(index) = anchorUs + (index - anchorIndex) * usPerSample.
// The index counts every sample the mic produced: buffers the driver drops
// when the reader falls behind are counted from I2S_EVENT_RX_Q_OVF and
// added to the index, so a stall leaves an honest gap, not a lag. Read
// latency (read time minus the clock's time for a block's last sample) is
// never negative for a correct clock, and its minimum over a window is the
// clock's error. Every kClockWindowMs the anchor absorbs that error and half
// of it goes into usPerSample, which tracks the I2S-vs-CPU rate difference.
namespace {
constexpr uint32_t kClockWindowMs = 500;
constexpr double kClockRateGain = 0.5;
constexpr double kClockMaxRateDeviation = 0.02;
}

uint32_t AudioSourceI2S::drainDroppedBuffers() {
    if (_i2sEvents == nullptr) {
        return 0;
    }
    uint32_t dropped = 0;
    i2s_event_t event;
    while (xQueueReceive(_i2sEvents, &event, 0) == pdTRUE) {
        if (event.type == I2S_EVENT_RX_Q_OVF) {
            ++dropped;
        }
    }
    return dropped;
}

uint32_t AudioSourceI2S::stampBlock(uint64_t blockStartIndex, size_t blockCount, uint32_t readAtUs) {
    const double nominalUsPerSample = _sampleRate > 0 ? 1000000.0 / static_cast<double>(_sampleRate) : 0.0;
    const uint32_t windowSamples = (static_cast<uint32_t>(_sampleRate) * kClockWindowMs) / 1000U;
    const double lastOffsetSamples = static_cast<double>(blockCount > 0 ? blockCount - 1U : 0U);
    if (!_clockAnchored) {
        _clockUsPerSample = nominalUsPerSample;
        _clockAnchorIndex = blockStartIndex;
        _clockAnchorUs = readAtUs - static_cast<uint32_t>(lastOffsetSamples * _clockUsPerSample);
        _clockWindowMinLatencyUs = INT32_MAX;
        _clockWindowSamples = 0;
        _clockAnchored = true;
    }

    const uint32_t startUs = _clockAnchorUs + static_cast<uint32_t>(
        static_cast<double>(blockStartIndex - _clockAnchorIndex) * _clockUsPerSample);
    const uint32_t lastUs = startUs + static_cast<uint32_t>(lastOffsetSamples * _clockUsPerSample);
    const int32_t latencyUs = static_cast<int32_t>(readAtUs - lastUs);
    if (latencyUs < _clockWindowMinLatencyUs) {
        _clockWindowMinLatencyUs = latencyUs;
    }
    _clockWindowSamples += static_cast<uint32_t>(blockCount);

    if (_clockWindowSamples >= windowSamples) {
        const int32_t correctionUs = _clockWindowMinLatencyUs;
        const uint64_t nextIndex = blockStartIndex + static_cast<uint64_t>(blockCount);
        const uint32_t nextUs = _clockAnchorUs + static_cast<uint32_t>(
            static_cast<double>(nextIndex - _clockAnchorIndex) * _clockUsPerSample);
        _clockUsPerSample += kClockRateGain * static_cast<double>(correctionUs)
            / static_cast<double>(_clockWindowSamples);
        const double lo = nominalUsPerSample * (1.0 - kClockMaxRateDeviation);
        const double hi = nominalUsPerSample * (1.0 + kClockMaxRateDeviation);
        _clockUsPerSample = _clockUsPerSample < lo ? lo : (_clockUsPerSample > hi ? hi : _clockUsPerSample);
        _clockAnchorUs = nextUs + static_cast<uint32_t>(correctionUs);
        _clockAnchorIndex = nextIndex;
        ++_stats.sampleClockCorrections;
        const uint32_t magnitudeUs = static_cast<uint32_t>(correctionUs < 0 ? -correctionUs : correctionUs);
        if (magnitudeUs > _stats.maxSampleClockCorrectionUs) {
            _stats.maxSampleClockCorrectionUs = magnitudeUs;
        }
        _stats.sampleClockRateMilliHz = static_cast<uint32_t>(1000000000.0 / _clockUsPerSample);
        _clockWindowMinLatencyUs = INT32_MAX;
        _clockWindowSamples = 0;
    }
    return startUs;
}

void AudioSourceI2S::resetStats() {
    _clockAnchored = false;
    _haveLastBlockEnd = false;
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

    const audio::PcmIntermediate diff =
        static_cast<audio::PcmIntermediate>(current) - static_cast<audio::PcmIntermediate>(_previousSample);
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

    uint8_t rawBytes[kRefillBatchSize * kFrameSlots * sizeof(int32_t)] = {};
    const size_t frameSize = kFrameSlots * static_cast<size_t>(bytesPerSample);
    const size_t requestedBytes = static_cast<size_t>(kRefillBatchSize) * frameSize;
    size_t bytesRead = 0;
    const esp_err_t readResult = i2s_read(kI2sPort, rawBytes, requestedBytes, &bytesRead, 0);
    recordReadAttempt(static_cast<int>(requestedBytes), static_cast<int>(bytesRead), readResult != ESP_OK && readResult != ESP_ERR_TIMEOUT);
    if (bytesRead == 0) {
        return false;
    }

    // Buffers the driver dropped since the last read happened before the data
    // just read; count them into the index so sample time stays true.
    const uint32_t droppedBuffers = drainDroppedBuffers();
    if (droppedBuffers > 0) {
        _stats.droppedDmaBuffers += droppedBuffers;
        // overflowCount had no producer before; dropped buffers are exactly
        // what it was meant to report (Analyzer marks such trials
        // buffer_overrun instead of judging a chirp with missing audio).
        _stats.overflowCount += droppedBuffers;
        _outputSampleIndex += static_cast<uint64_t>(droppedBuffers) * static_cast<uint64_t>(I2S_DMA_BUF_LEN);
    }

    const size_t fullSamplesRead = bytesRead / frameSize;
    const size_t samplesToProcess = fullSamplesRead < kRefillBatchSize ? fullSamplesRead : kRefillBatchSize;
    const uint32_t fillEndUs = micros();

    _blockCursor = 0;
    _blockStartSampleIndex = _outputSampleIndex;
    _blockCount = 0;
    for (size_t i = 0; i < samplesToProcess; ++i) {
        const uint8_t* samplePtr = rawBytes + (i * frameSize) + kMicSlot * static_cast<size_t>(bytesPerSample);
        const int32_t decodedSample = static_cast<int32_t>(decodePcmSample(samplePtr, bytesPerSample));
        _blockSamples[_blockCount++] = preprocessSample(decodedSample);
    }

    _blockApproxStartMicros = stampBlock(_blockStartSampleIndex, _blockCount, fillEndUs);
    if (_haveLastBlockEnd) {
        const int32_t deltaUs = static_cast<int32_t>(_blockApproxStartMicros - _lastBlockEndMicros);
        if (deltaUs < 0) {
            ++_stats.timestampBacksteps;
            const uint32_t backstepUs = static_cast<uint32_t>(-deltaUs);
            if (backstepUs > _stats.maxTimestampBackstepUs) {
                _stats.maxTimestampBackstepUs = backstepUs;
            }
        }
    }
    _lastBlockEndMicros = _blockApproxStartMicros + static_cast<uint32_t>(static_cast<double>(_blockCount) * _clockUsPerSample);
    _haveLastBlockEnd = true;
    _outputSampleIndex += static_cast<uint64_t>(_blockCount);
    _stats.totalSamplesRead += static_cast<uint64_t>(_blockCount);
    return _blockCount > 0;
}
