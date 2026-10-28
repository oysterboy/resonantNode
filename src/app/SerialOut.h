#pragma once

#include <Arduino.h>

/*
SerialOut (step 7a, ANA-004)

All console output goes through serialOut(), which collects a line in RAM and
hands it to Serial in one write at the newline (or when 1 KB is full).

Why: Arduino-ESP32 2.0.17 queues every Serial.write() call as its own item in
the UART TX ring buffer, with a header and 4-byte alignment. A 3-byte
print(" k=") costs ~25-30 bytes of the 16 KB ring, so a few KB of text in
small calls fill it, and from then on each call waits for the UART at
115200 baud. Measured on the Analyzer (bench/sessions/2026-10-22-ana004-timing):
- 2,000 small prints ran at exactly the UART rate after the first ~2 KB;
- a 6.6 KB SEQ trial report took ~180 ms;
- the I2S queue holds 24 ms, so every detected trial dropped 21-22 DMA
  buffers.
One write per line keeps the report to ~25 ring items.

Single-threaded: only the Arduino loop task prints. main.cpp flushes a partial
line at the end of every loop pass, so a prompt without a newline still goes
out. The host replay keeps printing straight to stdout.
*/
#if defined(HOST_REPLAY)

inline auto& serialOut() {
    return Serial;
}
inline void serialOutFlush() {}

#else

class LineBufferedSerial : public Print {
public:
    // A copy of every flushed line (or partial line) for a second sink: the
    // radio reply of a remote command (PAR-013, step 11a). nullptr = off.
    using Tee = void (*)(void* ctx, const uint8_t* data, size_t len);
    void setTee(Tee tee, void* ctx) {
        flush();  // what was printed before belongs to no reply
        _tee = tee;
        _teeCtx = ctx;
    }

    size_t write(uint8_t c) override {
        _buf[_len++] = c;
        if (c == '\n' || _len == sizeof(_buf)) {
            flush();
        }
        return 1;
    }

    size_t write(const uint8_t* data, size_t size) override {
        for (size_t i = 0; i < size; ++i) {
            write(data[i]);
        }
        return size;
    }

    void flush() override {
        if (_len > 0) {
            if (_tee != nullptr) {
                _tee(_teeCtx, _buf, _len);
            }
            Serial.write(_buf, _len);
            _len = 0;
        }
    }

    int availableForWrite() override { return Serial.availableForWrite(); }

private:
    uint8_t _buf[1024];
    size_t _len = 0;
    Tee _tee = nullptr;
    void* _teeCtx = nullptr;
};

inline LineBufferedSerial& serialOut() {
    static LineBufferedSerial out;
    return out;
}

inline void serialOutFlush() {
    serialOut().flush();
}

#endif
