#pragma once

class AudioSource {
public:
    virtual ~AudioSource() = default;
    virtual void begin() = 0;
    virtual int readSample() = 0;
};
