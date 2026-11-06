#pragma once

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <new>

/*
CaptureRing (ECHO-001)

The last N samples of the mic as int16, addressed by the absolute sample index (the count of samples pushed since
begin()), so a clip around an onset can be cut out later by the index the detector reported, as long as it has not
been overwritten yet. Fed once per sample from the mode's loop; pure, host-tested (test_capture_ring).

Owns: the buffer and the write index.
Does not: decide what to cut, or play anything.
*/
class CaptureRing {
public:
    // Allocates `samples` int16 (on the heap; static RAM is full). False when the allocation fails (capacity 0).
    bool begin(size_t samples) {
        _buffer.reset(new (std::nothrow) int16_t[samples]);
        _capacity = _buffer ? samples : 0;
        _written = 0;
        return _capacity > 0;
    }

    // Frees the buffer (before an OTA pull: the download needs the heap); begin() again to record.
    void end() {
        _buffer.reset();
        _capacity = 0;
        _written = 0;
    }

    void push(int16_t sample) {
        if (_capacity == 0) {
            return;
        }
        _buffer[static_cast<size_t>(_written % _capacity)] = sample;
        ++_written;
    }

    size_t capacity() const { return _capacity; }
    // The index the next push gets: every index below it has been written once.
    uint64_t written() const { return _written; }
    // The oldest index still held.
    uint64_t oldest() const { return _written > _capacity ? _written - _capacity : 0; }

    // Copies [from, from + count) into out. False (out untouched) when any of it is overwritten or not written yet.
    bool copy(uint64_t from, size_t count, int16_t* out) const {
        if (_capacity == 0 || from < oldest() || from + count > _written) {
            return false;
        }
        for (size_t k = 0; k < count; ++k) {
            out[k] = _buffer[static_cast<size_t>((from + k) % _capacity)];
        }
        return true;
    }

private:
    std::unique_ptr<int16_t[]> _buffer;
    size_t _capacity = 0;
    uint64_t _written = 0;
};
