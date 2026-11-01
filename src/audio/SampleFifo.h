#pragma once

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <memory>
#include <new>

/*
SampleFifo (ECHO-002, step 20.3)

int16 samples from one task to another, one writer and one reader, no lock: Whispernet's loop pushes what the delay
line plays, the I2S writer task pops it per DMA buffer. The indices are atomics (release on the writer's side,
acquire on the reader's), so a sample is in the buffer before the reader can see it. A full FIFO drops the new
sample, an empty one gives nothing; both are counted. Pure, host-tested (test_sample_fifo).

Owns: the buffer, the two indices, the drop and underrun counts.
Does not: pace either side (I2S does), or know what the samples are.
*/
class SampleFifo {
public:
    // Allocates capacity int16 (on the heap). False when the allocation fails.
    bool begin(size_t capacity) {
        _buffer.reset(new (std::nothrow) int16_t[capacity + 1]);
        _size = _buffer ? capacity + 1 : 0;  // one slot kept free: full and empty differ
        _head.store(0, std::memory_order_relaxed);
        _tail.store(0, std::memory_order_relaxed);
        _dropped = 0;
        _underruns = 0;
        return _size > 0;
    }

    // Writer side. False (counted) when full.
    bool push(int16_t sample) {
        if (_size == 0) {
            return false;
        }
        const size_t head = _head.load(std::memory_order_relaxed);
        const size_t next = (head + 1) % _size;
        if (next == _tail.load(std::memory_order_acquire)) {
            ++_dropped;
            return false;
        }
        _buffer[head] = sample;
        _head.store(next, std::memory_order_release);
        return true;
    }

    // Reader side: up to n samples into out; the count taken. Fewer than n is an underrun (counted once per call).
    size_t pop(int16_t* out, size_t n) {
        if (_size == 0) {
            return 0;
        }
        size_t tail = _tail.load(std::memory_order_relaxed);
        const size_t head = _head.load(std::memory_order_acquire);
        size_t k = 0;
        while (k < n && tail != head) {
            out[k++] = _buffer[tail];
            tail = (tail + 1) % _size;
        }
        _tail.store(tail, std::memory_order_release);
        if (k < n) {
            ++_underruns;
        }
        return k;
    }

    // Either side: the samples waiting (a snapshot).
    size_t available() const {
        if (_size == 0) {
            return 0;
        }
        const size_t head = _head.load(std::memory_order_acquire);
        const size_t tail = _tail.load(std::memory_order_acquire);
        return (head + _size - tail) % _size;
    }
    size_t capacity() const { return _size > 0 ? _size - 1 : 0; }
    uint32_t dropped() const { return _dropped; }      // writer's count
    uint32_t underruns() const { return _underruns; }  // reader's count

private:
    std::unique_ptr<int16_t[]> _buffer;
    size_t _size = 0;
    std::atomic<size_t> _head{0};  // the writer's next slot
    std::atomic<size_t> _tail{0};  // the reader's next slot
    uint32_t _dropped = 0;
    uint32_t _underruns = 0;
};
