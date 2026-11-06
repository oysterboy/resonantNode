#pragma once

#include <stddef.h>
#include <stdint.h>

/*
LongRecorder (2026-11-06): the mic as a stream of framed int16 PCM for the USB cable, for recordings of minutes
(the whisper corpus: labelled phrases of several voices on the board's own mic). The other captures are short: RAW
holds 375 ms, WHISPER REC two seconds as text.

A frame is 0xA5 0x5A, the position of its first sample in the recording (u32, little endian), the sample count
(u16), the samples (int16 little endian) and the 16-bit sum of every byte before it (u16). The position steps by the
count, so a frame the ring had no room for shows as a jump in the position (the tool fills it with silence) and
`droppedSamples()` counts it. tools/bench/longrec.py reads this.

Owns: the byte ring and the framing. Does not: the serial port or its baud rate (the shell), or the audio path.
Pure, host-tested (test_long_recorder).
*/
class LongRecorder {
public:
    static constexpr size_t kMaxFrameSamples = 120;
    static constexpr size_t kFrameOverhead = 10;  // magic 2, position 4, count 2, sum 2 after the samples

    // The ring of `capacityBytes` (the caller's storage, kept until the recording ends).
    void begin(uint8_t* storage, size_t capacityBytes, uint64_t totalSamples) {
        _buf = storage;
        _cap = capacityBytes;
        _head = 0;
        _count = 0;
        _position = 0;
        _total = totalSamples;
        _dropped = 0;
    }

    // The canonical 24-bit samples of one block, framed as int16 (>> 8). A frame that does not fit is dropped whole.
    void push(const int32_t* samples24, size_t count) {
        size_t i = 0;
        while (i < count && !complete()) {
            size_t n = count - i;
            n = n > kMaxFrameSamples ? kMaxFrameSamples : n;
            const uint64_t left = _total - _position;
            n = n > left ? static_cast<size_t>(left) : n;
            const size_t bytes = kFrameOverhead + 2U * n;
            if (_cap - _count >= bytes) {
                writeFrame(samples24 + i, n);
            } else {
                _dropped += n;
            }
            _position += n;
            i += n;
        }
    }

    size_t available() const { return _count; }
    // The next contiguous bytes for the port (up to the ring's end); consume() what was written.
    const uint8_t* peek(size_t& contiguous) const {
        contiguous = _head + _count > _cap ? _cap - _head : _count;
        return _buf + _head;
    }
    void consume(size_t n) {
        n = n > _count ? _count : n;
        _head = (_head + n) % _cap;
        _count -= n;
    }

    bool complete() const { return _position >= _total; }
    bool drained() const { return complete() && _count == 0; }
    uint64_t position() const { return _position; }
    uint64_t droppedSamples() const { return _dropped; }
    void stop() { _total = _position; }  // no more samples; the ring still drains

private:
    void put(uint8_t b) {
        _buf[(_head + _count) % _cap] = b;
        ++_count;
    }
    void writeFrame(const int32_t* s, size_t n) {
        uint16_t sum = 0;
        const auto out = [&](uint8_t b) {
            sum = static_cast<uint16_t>(sum + b);
            put(b);
        };
        out(0xA5);
        out(0x5A);
        for (int k = 0; k < 4; ++k) {
            out(static_cast<uint8_t>((_position >> (8 * k)) & 0xFFU));
        }
        out(static_cast<uint8_t>(n & 0xFFU));
        out(static_cast<uint8_t>((n >> 8) & 0xFFU));
        for (size_t i = 0; i < n; ++i) {
            const int32_t v = s[i] >> 8;
            const int16_t c = static_cast<int16_t>(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
            out(static_cast<uint8_t>(c & 0xFF));
            out(static_cast<uint8_t>((static_cast<uint16_t>(c) >> 8) & 0xFF));
        }
        put(static_cast<uint8_t>(sum & 0xFFU));
        put(static_cast<uint8_t>((sum >> 8) & 0xFFU));
    }

    uint8_t* _buf = nullptr;
    size_t _cap = 0;
    size_t _head = 0;
    size_t _count = 0;
    uint64_t _position = 0;
    uint64_t _total = 0;
    uint64_t _dropped = 0;
};
