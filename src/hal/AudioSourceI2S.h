#pragma once

#include <cstddef>
#include <stdint.h>
#include <memory>

#include "../audio/AudioSource.h"

struct AudioSlotDiagnostics {
    bool present = false;
    const char* slotDiagSource = "post_mono_pcm";
    unsigned long slotCount[2] = {0, 0};
    int slotMin[2] = {0, 0};
    int slotMax[2] = {0, 0};
    double slotSumSquares[2] = {0.0, 0.0};
    unsigned long slotRepeatedRun[2] = {0, 0};
    long slotSignedRange[2] = {0, 0};
    const char* chosenSlot = "none";
    const char* activeSlot = "none";
    const char* slotSelectionReason = "none";
};

struct AudioPhilipsDiagnostics {
    bool present = false;
    unsigned long sampleCount = 0;
    unsigned long phaseCount[64] = {0};
    double phaseAbsDeltaSum[64] = {0.0};
    int phaseMaxAbsDelta[64] = {0};
    int maxDelta = 0;
    unsigned long maxDeltaAbs = 0;
    uint64_t maxDeltaGlobalIndex = 0;
    size_t maxDeltaMod64 = 0;
    unsigned long maxDeltaReadIndex = 0;
    unsigned long maxDeltaSelectedInRead = 0;
    int rawBefore = 0;
    int rawAfter = 0;
};

/*
AudioSourceI2S

ESP32 I2S implementation of AudioSource.
Owns I2S setup, block reads, raw-sample diagnostics, and approximate block timing.
Does not know about AudioSignal, DetectionRuntime, Analyzer, or Behavior.
*/
class AudioSourceI2S : public AudioSource {
public:
    enum class TransportMode {
        ArduinoWrapper,
        DirectDriver,
    };

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
    uint32_t lastRawWord() const override;
    bool setI2SFrameMode(I2SFrameMode mode) override;
    I2SFrameMode i2sFrameMode() const override;
    bool lastSampleWasBlockStart() const override;
    const AudioSourceStats& stats() const override;
    const AudioSlotDiagnostics& slotDiagnostics() const;
    const AudioPhilipsDiagnostics& philipsDiagnostics() const;
    void setTransportMode(TransportMode mode);
    TransportMode transportMode() const;
    void setReadChunkBytes(size_t bytes);
    size_t readChunkBytes() const;
    void setDmaBufferFrames(size_t frames);
    size_t dmaBufferFrames() const;
    void resetStats() override;
    uint32_t samplePeriodUs() const;

private:
    bool refillBlock();
    void recordReadAttempt(int requestedBytes, int bytesRead, bool readError);
    void logPhilipsReadHeader(size_t readCallIndex,
                              int requestedBytes,
                              int bytesRead,
                              size_t wordsRead,
                              size_t frameCount,
                              size_t remainderBytes,
                              size_t remainderWords,
                              size_t firstExpectedSlotPhase,
                              size_t lastConsumedSlotPhase,
                              size_t emittedSamples) const;
    void logPhilipsBoundarySample(size_t readCallIndex,
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
                                  size_t frameCount) const;
    bool shouldLogPhilipsBoundarySample(size_t readCallIndex,
                                       size_t selectedIndexInRead,
                                       size_t rawWordIndexInRead,
                                       uint64_t globalSelectedIndex) const;

    int _sckPin;
    int _fsPin;
    int _dataInPin;
    int _sampleRate;
    int _bitsPerSample;
    bool _started = false;
    // Large enough for a 1024-byte Philips read: 256 decoded 32-bit words.
    static constexpr size_t kRefillBatchSize = 256;
    std::unique_ptr<int32_t[]> _blockSamples;
    std::unique_ptr<uint32_t[]> _blockRawWords;
    size_t _blockCount = 0;
    size_t _blockCursor = 0;
    uint64_t _blockStartSampleIndex = 0;
    uint32_t _blockApproxStartMicros = 0;
    bool _blockOverflowBeforeBlock = false;
    unsigned long _droppedSamples = 0;
    size_t _maxBufferedSamples = 0;
    uint32_t _samplePeriodUs = 0;
    uint64_t _outputSampleIndex = 0;
    int _selectedSlotIndex = -1;
    uint32_t _lastRawWord = 0;
    bool _lastReadWasBlockStart = false;
    I2SFrameMode _frameMode = I2SFrameMode::LeftJustifiedAllSlots;
    uint64_t _debugReadCallIndex = 0;
    uint64_t _debugPhilipsSelectedSampleIndex = 0;
    int _debugPhilipsPrevDecodedPcm = 0;
    bool _debugPhilipsHasPrevDecodedPcm = false;
    size_t _debugPhilipsExpectedSlotPhase = 0;
    TransportMode _transportMode = TransportMode::ArduinoWrapper;
    size_t _readChunkBytes = 512;
    size_t _dmaBufferFrames = 128;
    AudioPhilipsDiagnostics _philipsDiagnostics;
    AudioSourceStats _stats;
    AudioSlotDiagnostics _slotDiagnostics;
};
