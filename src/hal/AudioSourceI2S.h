#pragma once

#include "AudioSource.h"

class AudioSourceI2S : public AudioSource {
public:
    AudioSourceI2S(int sckPin, int fsPin, int dataInPin, int sampleRate = 16000, int bitsPerSample = 32);

    void begin() override;
    int readSample() override;

private:
    int _sckPin;
    int _fsPin;
    int _dataInPin;
    int _sampleRate;
    int _bitsPerSample;
    bool _started = false;
};
