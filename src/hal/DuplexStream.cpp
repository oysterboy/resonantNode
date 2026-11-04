#include "DuplexStream.h"

#include <new>

namespace {
constexpr float kFullScale = 2147483647.0f;
constexpr uint32_t kWriteTimeoutMs = 0;  // never wait: the half-full queue has room for one block
}  // namespace

bool DuplexStream::begin(size_t recordFrames) {
    _record.reset(new (std::nothrow) int16_t[recordFrames]);
    _written.reset(new (std::nothrow) bool[recordFrames]);
    if (!_record || !_written || !_port.txEnabled()) {
        _frames = 0;
        return false;
    }
    _frames = recordFrames;
    for (size_t i = 0; i < _frames; ++i) {
        _record[i] = 0;
        _written[i] = false;
    }
    writeSilence(kQueueFrames);  // half the TX queue: from here on one block out per block in keeps it there
    return true;
}

void DuplexStream::writeSilence(size_t frames) {
    for (size_t i = 0; i < 2 * kMaxBlock; ++i) {
        _out[i] = 0;
    }
    size_t left = frames;
    while (left > 0) {
        const size_t n = left < kMaxBlock ? left : kMaxBlock;
        size_t written = 0;
        if (!_port.writeTx(_out, n * 2 * sizeof(int32_t), written, 50) || written == 0) {
            break;
        }
        const size_t done = written / (2 * sizeof(int32_t));
        left -= done < left ? done : left;
    }
}

void DuplexStream::writeBlock(uint64_t rxIndex, const int16_t* samples, size_t n) {
    if (_frames == 0) {
        return;
    }
    n = n > kMaxBlock ? kMaxBlock : n;
    if (_end != 0 && rxIndex != _end) {
        // The mic dropped buffers: the loop stalled and the speaker ran the queue down; fill it back to the half.
        writeSilence(kQueueFrames);
        ++_refills;
    }
    for (size_t i = 0; i < n; ++i) {
        float f = static_cast<float>(samples[i]) / 32768.0f * _gain;
        f = f > 0.999f ? 0.999f : (f < -0.999f ? -0.999f : f);  // never past int32 (issue #31's trap)
        const int32_t v = static_cast<int32_t>(f * kFullScale);
        _out[2 * i] = v;
        _out[2 * i + 1] = v;  // both slots: the amp's channel select does not matter
        const size_t slot = static_cast<size_t>((rxIndex + i) % _frames);
        _record[slot] = static_cast<int16_t>(f * 32767.0f);
        _written[slot] = true;
    }
    // Slots skipped since the last block (the mic dropped buffers): nothing was written for those indices.
    for (uint64_t k = rxIndex > _end + _frames ? rxIndex - _frames : _end; k < rxIndex; ++k) {
        _written[static_cast<size_t>(k % _frames)] = false;
    }
    _end = rxIndex + n;
    size_t written = 0;
    const size_t bytes = n * 2 * sizeof(int32_t);
    if (!_port.writeTx(_out, bytes, written, kWriteTimeoutMs) || written < bytes) {
        ++_shortWrites;
    }
}

bool DuplexStream::played(uint64_t rxIndex, int16_t& out) const {
    if (_frames == 0 || rxIndex >= _end || rxIndex + _frames < _end) {
        return false;
    }
    const size_t slot = static_cast<size_t>(rxIndex % _frames);
    if (!_written[slot]) {
        return false;
    }
    out = _record[slot];
    return true;
}
