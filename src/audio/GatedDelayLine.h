#pragma once

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <new>

/*
GatedDelayLine (ECHO-002, step 20.3)

Whispernet's output path: the mic delayed by D samples (up to the capacity allocated), passed only where a gate bit is set. The bits are per block
of input samples and can be set after the samples came in, as long as they have not been played yet: the gate opens
on a provisional verdict ~0.3 s into a phrase and passes the phrase from its start, which the line still holds when
D is longer than the evidence time. The gain ramps toward each block's bit over rampSamples, so the gate opens and
closes without a click. Pure, host-tested (test_gated_delay).

Owns: the sample buffer, the gate bits, the gain ramp, the write index.
Does not: decide when the gate opens (the mode's rule on Detection's verdicts), or drive the I2S output.
*/
class GatedDelayLine {
public:
    // Allocates capacitySamples int16 and a byte per block (on the heap; static RAM is full); the delay starts at the
    // capacity. False when the allocation fails. blockSamples is the gate's granularity, rampSamples the gain's ramp
    // (0: a hard switch).
    bool begin(size_t capacitySamples, size_t blockSamples, size_t rampSamples) {
        _capacity = capacitySamples;
        _delay = capacitySamples;
        _block = blockSamples > 0 ? blockSamples : 1;
        _blocks = _capacity / _block + 2;  // the block being written never aliases the block being played
        _samples.reset(new (std::nothrow) int16_t[_capacity > 0 ? _capacity : 1]);
        _bits.reset(new (std::nothrow) uint8_t[_blocks]);
        if (!_samples || !_bits || _capacity == 0) {
            _capacity = 0;
            _delay = 0;
            return false;
        }
        for (size_t i = 0; i < _capacity; ++i) {
            _samples[i] = 0;
        }
        for (size_t i = 0; i < _blocks; ++i) {
            _bits[i] = 0;
        }
        _step = rampSamples > 0 ? 1.0f / static_cast<float>(rampSamples) : 1.0f;
        _gain = 0.0f;
        _written = 0;
        _open = false;
        return true;
    }

    // Frees the buffers (capacity 0, process() gives silence); begin() allocates them again.
    void end() {
        _samples.reset();
        _bits.reset();
        _capacity = 0;
        _delay = 0;
        _open = false;
    }

    // The delay, 1 .. the capacity (Whispernet's per-node D). A change mid-stream replays or skips a stretch once.
    void setDelay(size_t samples) {
        if (_capacity == 0) {
            return;
        }
        _delay = samples < 1 ? 1 : (samples > _capacity ? _capacity : samples);
    }

    // While open, every block that comes in is marked to pass (the gate as it stands now).
    void setOpen(bool open) {
        _open = open;
        if (open && _delay > 0) {
            _bits[blockSlot(_written)] = 1;  // the block being written now
        }
    }
    bool isOpen() const { return _open; }

    // Marks the blocks covering input samples [from, to) to pass. Samples already played or not yet written are left
    // out: the past cannot be passed after it was played, the future is setOpen's.
    void pass(uint64_t from, uint64_t to) {
        if (_delay == 0) {
            return;
        }
        const uint64_t firstUnplayed = _written > _delay ? _written - _delay : 0;
        from = from > firstUnplayed ? from : firstUnplayed;
        to = to < _written ? to : _written;
        for (uint64_t b = from / _block; from < to && b <= (to - 1) / _block; ++b) {
            _bits[static_cast<size_t>(b % _blocks)] = 1;
        }
    }

    // One sample in; out, the sample from delaySamples ago times the gate's gain (0 until the line has filled).
    int16_t process(int16_t in) {
        if (_delay == 0) {
            return 0;
        }
        if (_written % _block == 0) {
            _bits[blockSlot(_written)] = _open ? 1 : 0;  // a new input block: the gate as it stands
        }
        int16_t delayed = 0;
        float target = 0.0f;
        if (_written >= _delay) {
            delayed = _samples[static_cast<size_t>((_written - _delay) % _capacity)];  // read before the write below
            target = _bits[blockSlot(_written - _delay)] != 0 ? 1.0f : 0.0f;
        }
        _samples[static_cast<size_t>(_written % _capacity)] = in;
        ++_written;
        if (_gain < target) {
            _gain = _gain + _step < target ? _gain + _step : target;
        } else if (_gain > target) {
            _gain = _gain - _step > target ? _gain - _step : target;
        }
        const float out = static_cast<float>(delayed) * _gain;
        return static_cast<int16_t>(out >= 0.0f ? out + 0.5f : out - 0.5f);
    }

    size_t delay() const { return _delay; }
    size_t capacity() const { return _capacity; }
    // The input sample at an absolute index, while the line still holds it (the last capacity samples written).
    bool sampleAt(uint64_t index, int16_t& out) const {
        if (_capacity == 0 || index >= _written || index + _capacity < _written) {
            return false;
        }
        out = _samples[static_cast<size_t>(index % _capacity)];
        return true;
    }
    // The index the next input sample gets.
    uint64_t written() const { return _written; }
    float gain() const { return _gain; }

private:
    size_t blockSlot(uint64_t sample) const { return static_cast<size_t>((sample / _block) % _blocks); }

    std::unique_ptr<int16_t[]> _samples;
    std::unique_ptr<uint8_t[]> _bits;
    size_t _capacity = 0;
    size_t _delay = 0;
    size_t _block = 1;
    size_t _blocks = 0;
    float _step = 1.0f;
    float _gain = 0.0f;
    uint64_t _written = 0;
    bool _open = false;
};
