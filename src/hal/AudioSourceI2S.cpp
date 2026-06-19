#include "AudioSourceI2S.h"
#include <I2S.h>
#include <math.h>
#include <string.h>
#include <new>

namespace {

constexpr bool kPhilipsBoundaryDiagnosticsEnabled = true;
constexpr size_t kPhilipsDiagnosticReadLimit = 5;

int decodePcmSample(const uint8_t* samplePtr, int bytesPerSample) {
    if (samplePtr == nullptr || bytesPerSample <= 0) {
        return 0;
    }

    if (bytesPerSample >= static_cast<int>(sizeof(int32_t))) {
        int32_t sample32 = 0;
        memcpy(&sample32, samplePtr, sizeof(sample32));
        // The left-justified transport appears to place the useful sample in bits [23:8].
        // Preserve the sign while discarding the empty low byte.
        return static_cast<int>(sample32 >> 8);
    }

    if (bytesPerSample == 3) {
        uint32_t packed = 0;
        memcpy(&packed, samplePtr, 3);
        if ((packed & 0x00800000UL) != 0) {
            packed |= 0xFF000000UL;
        }
        return static_cast<int>(static_cast<int32_t>(packed));
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

const char* slotName(size_t slotIndex) {
    return slotIndex == 0 ? "slot0" : "slot1";
}

bool slotHasMeaningfulVariation(unsigned long count, long signedRange) {
    return count > 0 && signedRange > 0;
}

bool isPhilipsBoundarySample(uint64_t globalSelectedIndex) {
    const uint64_t phase = globalSelectedIndex % 64ULL;
    return (phase >= 30ULL && phase <= 34ULL)
        || phase == 62ULL
        || phase == 63ULL
        || phase == 0ULL
        || phase == 1ULL
        || phase == 2ULL;
}
}

AudioSourceI2S::AudioSourceI2S(int sckPin, int fsPin, int dataInPin, int sampleRate, int bitsPerSample)
    : _sckPin(sckPin),
      _fsPin(fsPin),
      _dataInPin(dataInPin),
      _sampleRate(sampleRate),
      _bitsPerSample(bitsPerSample),
      _blockSamples(new (std::nothrow) int32_t[kRefillBatchSize] {}),
      _blockRawWords(new (std::nothrow) uint32_t[kRefillBatchSize] {}) {}

void AudioSourceI2S::begin() {
    // Keep the public contract sample-based even though I2S may buffer internally.
    resetStats();
    if (!_blockSamples) {
        _blockSamples.reset(new (std::nothrow) int32_t[kRefillBatchSize] {});
    }
    if (!_blockRawWords) {
        _blockRawWords.reset(new (std::nothrow) uint32_t[kRefillBatchSize] {});
    }
    _started = false;
    _samplePeriodUs = _sampleRate > 0 ? static_cast<uint32_t>(1000000UL / static_cast<uint32_t>(_sampleRate)) : 0;

    I2S.end();
    I2S.setAllPins(_sckPin, _fsPin, _dataInPin, I2S_PIN_NO_CHANGE, I2S_PIN_NO_CHANGE);
    const bool carryAllSlots = _frameMode == I2SFrameMode::LeftJustifiedAllSlots;
    // Mode 1 keeps the left-justified transport and carries both slots.
    // Mode 2 switches to Philips framing and keeps only the left slot.
    const int beginResult = I2S.begin(carryAllSlots ? I2S_LEFT_JUSTIFIED_MODE : I2S_PHILIPS_MODE,
                                      _sampleRate,
                                      carryAllSlots ? _bitsPerSample : 32);
    _started = beginResult != 0;
}

bool AudioSourceI2S::shouldLogPhilipsBoundarySample(size_t,
                                                    size_t,
                                                    size_t,
                                                    uint64_t) const {
    return false;
}

void AudioSourceI2S::logPhilipsReadHeader(size_t readCallIndex,
                                          int requestedBytes,
                                          int bytesRead,
                                          size_t wordsRead,
                                          size_t frameCount,
                                          size_t remainderBytes,
                                          size_t remainderWords,
                                          size_t firstExpectedSlotPhase,
                                          size_t lastConsumedSlotPhase,
                                          size_t emittedSamples) const {
    (void)readCallIndex;
    (void)requestedBytes;
    (void)bytesRead;
    (void)wordsRead;
    (void)frameCount;
    (void)remainderBytes;
    (void)remainderWords;
    (void)firstExpectedSlotPhase;
    (void)lastConsumedSlotPhase;
    (void)emittedSamples;
}

void AudioSourceI2S::logPhilipsBoundarySample(size_t readCallIndex,
                                              size_t selectedIndexInRead,
                                              size_t rawWordIndexInRead,
                                              size_t selectedSlotIndex,
                                              uint32_t slot0RawWord,
                                              uint32_t slot1RawWord,
                                              int decodedPcm,
                                              int previousDecodedPcm,
                                              size_t bytesRequested,
                                              int bytesRead,
                                              size_t wordsRead,
                                              size_t frameCount) const {
    (void)readCallIndex;
    (void)selectedIndexInRead;
    (void)rawWordIndexInRead;
    (void)selectedSlotIndex;
    (void)slot0RawWord;
    (void)slot1RawWord;
    (void)decodedPcm;
    (void)previousDecodedPcm;
    (void)bytesRequested;
    (void)bytesRead;
    (void)wordsRead;
    (void)frameCount;
}

bool AudioSourceI2S::available() {
    return _blockCursor < _blockCount || refillBlock();
}

int AudioSourceI2S::availableBytes() const {
    return I2S.available();
}

uint32_t AudioSourceI2S::lastRawWord() const {
    return _lastRawWord;
}

bool AudioSourceI2S::setI2SFrameMode(I2SFrameMode mode) {
    if (_frameMode == mode) {
        return false;
    }

    _frameMode = mode;
    if (_started) {
        begin();
    }
    return true;
}

AudioSourceI2S::I2SFrameMode AudioSourceI2S::i2sFrameMode() const {
    return _frameMode;
}

bool AudioSourceI2S::lastSampleWasBlockStart() const {
    return _lastReadWasBlockStart;
}

bool AudioSourceI2S::readSample(int& sample, uint32_t& sampleTimeUs) {
    if (_blockCursor >= _blockCount && !refillBlock()) {
        return false;
    }

    if (_blockCursor >= _blockCount) {
        return false;
    }

    const size_t index = _blockCursor++;
    _lastReadWasBlockStart = index == 0;
    sample = static_cast<int>(_blockSamples[index]);
    if (_blockRawWords) {
        _lastRawWord = _blockRawWords[index];
    }
    sampleTimeUs = _blockApproxStartMicros + sampleOffsetUs(static_cast<uint32_t>(index), static_cast<uint32_t>(_sampleRate));
    return true;
}

bool AudioSourceI2S::readRawSample(int& sample, uint32_t& sampleTimeUs) {
    // Debug/raw-capture compatibility path; normal detector and RAW CSV should use readSample().
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

    sample = decodePcmSample(rawBytes, bytesPerSample);
    {
        uint32_t rawWord = 0;
        memcpy(&rawWord, rawBytes, bytesPerSample < static_cast<int>(sizeof(rawWord)) ? static_cast<size_t>(bytesPerSample) : sizeof(rawWord));
        _lastRawWord = rawWord;
    }
    _lastReadWasBlockStart = false;
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
    _lastReadWasBlockStart = _blockCursor == 0;
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

const AudioSlotDiagnostics& AudioSourceI2S::slotDiagnostics() const {
    return _slotDiagnostics;
}

const AudioPhilipsDiagnostics& AudioSourceI2S::philipsDiagnostics() const {
    return _philipsDiagnostics;
}

void AudioSourceI2S::resetStats() {
    _blockCursor = 0;
    _blockCount = 0;
    _blockStartSampleIndex = 0;
    _blockApproxStartMicros = 0;
    _blockOverflowBeforeBlock = false;
    _droppedSamples = 0;
    _maxBufferedSamples = 0;
    _outputSampleIndex = 0;
    _selectedSlotIndex = -1;
    _lastRawWord = 0;
    _lastReadWasBlockStart = false;
    _debugReadCallIndex = 0;
    _debugPhilipsSelectedSampleIndex = 0;
    _debugPhilipsPrevDecodedPcm = 0;
    _debugPhilipsHasPrevDecodedPcm = false;
    _debugPhilipsExpectedSlotPhase = 0;
    _philipsDiagnostics = {};
    _stats = {};
    _slotDiagnostics = {};
    _slotDiagnostics.slotDiagSource = "pcm_i2s_words";
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

    const size_t readCallIndex = _debugReadCallIndex++;

    const size_t fullSamplesRead = static_cast<size_t>(bytesRead) / static_cast<size_t>(bytesPerSample);
    const size_t sampleBytes = static_cast<size_t>(bytesPerSample);
    const size_t samplesToProcess = fullSamplesRead;
    const size_t wordsRead = fullSamplesRead;
    const size_t frameCount = wordsRead / 2U;
    const size_t remainderBytes = static_cast<size_t>(bytesRead % 8);
    const size_t remainderWords = wordsRead % 2U;
    int32_t rawSamples[kRefillBatchSize] = {};
    uint32_t rawWords[kRefillBatchSize] = {};
    double slotSumSquares[2] = {0.0, 0.0};
    unsigned long slotCount[2] = {0, 0};
    int slotMin[2] = {0, 0};
    int slotMax[2] = {0, 0};
    bool slotHasValue[2] = {false, false};
    int slotLastValue[2] = {0, 0};
    unsigned long slotCurrentRun[2] = {0, 0};
    unsigned long slotRepeatedRun[2] = {0, 0};
    _blockCount = 0;
    _blockCursor = 0;
    _blockStartSampleIndex = _outputSampleIndex;
    _blockOverflowBeforeBlock = _droppedSamples > 0;
    for (size_t i = 0; i < samplesToProcess; ++i) {
        const uint8_t* samplePtr = rawBytes + (i * sampleBytes);
        const int rawSample = decodePcmSample(samplePtr, bytesPerSample);

        rawSamples[i] = rawSample;
        uint32_t rawWord = 0;
        memcpy(&rawWord, samplePtr, sampleBytes < static_cast<int>(sizeof(rawWord)) ? static_cast<size_t>(sampleBytes) : sizeof(rawWord));
        rawWords[i] = rawWord;

        const int sample = rawSample;
        const size_t slotIndex = i % 2U;
        const size_t diagSlotIndex = slotIndex < 2U ? slotIndex : 1U;
        slotSumSquares[diagSlotIndex] += static_cast<double>(sample) * static_cast<double>(sample);
        ++slotCount[diagSlotIndex];
        if (!slotHasValue[diagSlotIndex]) {
            slotHasValue[diagSlotIndex] = true;
            slotMin[diagSlotIndex] = sample;
            slotMax[diagSlotIndex] = sample;
            slotLastValue[diagSlotIndex] = sample;
            slotCurrentRun[diagSlotIndex] = 1;
            slotRepeatedRun[diagSlotIndex] = 1;
        } else {
            if (sample < slotMin[diagSlotIndex]) {
                slotMin[diagSlotIndex] = sample;
            }
            if (sample > slotMax[diagSlotIndex]) {
                slotMax[diagSlotIndex] = sample;
            }
            if (sample == slotLastValue[diagSlotIndex]) {
                ++slotCurrentRun[diagSlotIndex];
            } else {
                slotCurrentRun[diagSlotIndex] = 1;
                slotLastValue[diagSlotIndex] = sample;
            }
            if (slotCurrentRun[diagSlotIndex] > slotRepeatedRun[diagSlotIndex]) {
                slotRepeatedRun[diagSlotIndex] = slotCurrentRun[diagSlotIndex];
            }
            slotLastValue[diagSlotIndex] = sample;
        }
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
    _slotDiagnostics.present = slotCount[0] > 0 || slotCount[1] > 0;
    _slotDiagnostics.slotDiagSource = "pcm_i2s_words";
    for (size_t slot = 0; slot < 2; ++slot) {
        _slotDiagnostics.slotCount[slot] = slotCount[slot];
        _slotDiagnostics.slotMin[slot] = slotHasValue[slot] ? slotMin[slot] : 0;
        _slotDiagnostics.slotMax[slot] = slotHasValue[slot] ? slotMax[slot] : 0;
        _slotDiagnostics.slotSumSquares[slot] = slotSumSquares[slot];
        _slotDiagnostics.slotRepeatedRun[slot] = slotRepeatedRun[slot];
        _slotDiagnostics.slotSignedRange[slot] = slotHasValue[slot]
            ? static_cast<long>(slotMax[slot] - slotMin[slot])
            : 0L;
    }
    if (!_slotDiagnostics.present) {
        _slotDiagnostics.chosenSlot = "none";
        _slotDiagnostics.activeSlot = "none";
        _slotDiagnostics.slotSelectionReason = "no_data";
    } else {
        const double rms0 = slotCount[0] > 0 ? sqrt(slotSumSquares[0] / static_cast<double>(slotCount[0])) : 0.0;
        const double rms1 = slotCount[1] > 0 ? sqrt(slotSumSquares[1] / static_cast<double>(slotCount[1])) : 0.0;
        const long range0 = _slotDiagnostics.slotSignedRange[0];
        const long range1 = _slotDiagnostics.slotSignedRange[1];
        const bool alive0 = slotHasMeaningfulVariation(slotCount[0], range0);
        const bool alive1 = slotHasMeaningfulVariation(slotCount[1], range1);
        if (_selectedSlotIndex < 0) {
            if (!alive0 && !alive1) {
                _selectedSlotIndex = 0;
                _slotDiagnostics.slotSelectionReason = "dead_slots_fallback_slot0";
            } else if (alive0 != alive1) {
                _selectedSlotIndex = alive0 ? 0 : 1;
                _slotDiagnostics.slotSelectionReason = "reject_dead_slot";
            } else if (range0 != range1) {
                _selectedSlotIndex = range0 > range1 ? 0 : 1;
                _slotDiagnostics.slotSelectionReason = "signed_range";
            } else if (rms0 >= rms1) {
                _selectedSlotIndex = 0;
                _slotDiagnostics.slotSelectionReason = "rms_tie_break";
            } else {
                _selectedSlotIndex = 1;
                _slotDiagnostics.slotSelectionReason = "rms";
            }
        }
        _slotDiagnostics.chosenSlot = slotName(static_cast<size_t>(_selectedSlotIndex));
        _slotDiagnostics.activeSlot = (alive0 && alive1) ? "both" : slotName(static_cast<size_t>(_selectedSlotIndex));
    }
    const uint32_t fillEndUs = micros();
    const bool carryAllSlots = _frameMode == I2SFrameMode::LeftJustifiedAllSlots;
    if (carryAllSlots) {
        _blockCount = 0;
        for (size_t i = 0; i < samplesToProcess && _blockCount < kRefillBatchSize; ++i) {
            _blockSamples[_blockCount++] = rawSamples[i];
            _blockRawWords[_blockCount - 1] = rawWords[i];
            _lastRawWord = rawWords[i];
        }
        const uint32_t selectedFrameAge = _blockCount > 0 ? static_cast<uint32_t>(_blockCount - 1U) : 0U;
        _blockApproxStartMicros = fillEndUs > sampleOffsetUs(selectedFrameAge, static_cast<uint32_t>(_sampleRate))
            ? fillEndUs - sampleOffsetUs(selectedFrameAge, static_cast<uint32_t>(_sampleRate))
            : 0U;
        _slotDiagnostics.chosenSlot = "raw_passthrough";
        _slotDiagnostics.activeSlot = "raw_passthrough";
        _slotDiagnostics.slotSelectionReason = "left_justified_all_slots";
    } else {
        const size_t chosenSlotIndex = _selectedSlotIndex == 1 ? 1U : 0U;
        const size_t selectedFrameCount = frameCount;
        const uint32_t selectedFrameAge = selectedFrameCount > 0 ? static_cast<uint32_t>(selectedFrameCount - 1U) : 0U;
        _blockApproxStartMicros = fillEndUs > sampleOffsetUs(selectedFrameAge, static_cast<uint32_t>(_sampleRate))
            ? fillEndUs - sampleOffsetUs(selectedFrameAge, static_cast<uint32_t>(_sampleRate))
            : 0U;
        _blockCount = 0;
        _philipsDiagnostics.present = true;
        int previousSelectedPcm = _debugPhilipsHasPrevDecodedPcm ? _debugPhilipsPrevDecodedPcm : 0;
        bool hasPreviousSelectedPcm = _debugPhilipsHasPrevDecodedPcm;
        for (size_t frame = 0; frame < selectedFrameCount && _blockCount < kRefillBatchSize; ++frame) {
            const size_t rawIndex = (frame * 2U) + chosenSlotIndex;
            if (rawIndex >= samplesToProcess) {
                break;
            }
            const int currentSelectedPcm = rawSamples[rawIndex];
            const uint64_t globalSelectedIndex = _debugPhilipsSelectedSampleIndex + static_cast<uint64_t>(frame);
            const size_t phase = static_cast<size_t>(globalSelectedIndex % 64ULL);
            if (hasPreviousSelectedPcm) {
                const int delta = currentSelectedPcm - previousSelectedPcm;
                const unsigned long absDelta = static_cast<unsigned long>(delta < 0 ? -static_cast<int64_t>(delta) : static_cast<int64_t>(delta));
                ++_philipsDiagnostics.sampleCount;
                ++_philipsDiagnostics.phaseCount[phase];
                _philipsDiagnostics.phaseAbsDeltaSum[phase] += static_cast<double>(absDelta);
                if (static_cast<int>(absDelta) > _philipsDiagnostics.phaseMaxAbsDelta[phase]) {
                    _philipsDiagnostics.phaseMaxAbsDelta[phase] = static_cast<int>(absDelta);
                }
                if (absDelta > _philipsDiagnostics.maxDeltaAbs) {
                    _philipsDiagnostics.maxDeltaAbs = absDelta;
                    _philipsDiagnostics.maxDelta = delta;
                    _philipsDiagnostics.maxDeltaGlobalIndex = globalSelectedIndex;
                    _philipsDiagnostics.maxDeltaMod64 = phase;
                    _philipsDiagnostics.maxDeltaReadIndex = static_cast<unsigned long>(readCallIndex);
                    _philipsDiagnostics.maxDeltaSelectedInRead = static_cast<unsigned long>(frame);
                    _philipsDiagnostics.rawBefore = previousSelectedPcm;
                    _philipsDiagnostics.rawAfter = currentSelectedPcm;
                }
            }
            _blockSamples[_blockCount] = currentSelectedPcm;
            _blockRawWords[_blockCount] = rawWords[rawIndex];
            _lastRawWord = rawWords[rawIndex];
            ++_blockCount;
            previousSelectedPcm = currentSelectedPcm;
            hasPreviousSelectedPcm = true;
            _debugPhilipsPrevDecodedPcm = currentSelectedPcm;
            _debugPhilipsHasPrevDecodedPcm = true;
        }
        _slotDiagnostics.chosenSlot = "slot0";
        _slotDiagnostics.activeSlot = "slot0";
        _slotDiagnostics.slotSelectionReason = "philips_left_slot";

        if (_blockCount > 0) {
            _debugPhilipsExpectedSlotPhase = chosenSlotIndex;
            _debugPhilipsSelectedSampleIndex += static_cast<uint64_t>(_blockCount);
        }
    }
    _outputSampleIndex += static_cast<uint64_t>(_blockCount);
    return _blockCount > 0;
}
