#pragma once

class AnalogInHal {
public:
    explicit AnalogInHal(int pin);
    void begin();
    int readRaw() const;

private:
    int _pin;
};