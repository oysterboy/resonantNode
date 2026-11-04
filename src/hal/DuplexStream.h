#pragma once

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "AudioSourceI2S.h"

/*
DuplexStream (ECHO-002, 2026-11-04)

The whisper build's output in lockstep with its input: for every mic block the mode reads, it writes exactly as many
output frames, so the speaker runs a fixed number of frames behind the mic, and it keeps what it wrote under the mic
block's indices. The TX queue is filled half with silence at begin() and kept there: one block out per block in on
the same I2S clock, written without waiting (a full queue made every write wait for the clock and the loop dropped
mic buffers, E5 2026-11-04). When the mic drops buffers (the loop stalled, so the speaker ran the queue down) the
half queue of silence is written again, so the offset comes back. The echo canceller reads played(rx - delay): the sample written for an earlier mic index
(decisions/2026-11-04-whispernet-duplex-lockstep.md, myspec 7.3).

Owns: the port's writing in that build, the gain into the amp, the played record.
Does not: read the port (the mode reads AudioSourceI2S as before), decide what to play (Whispernet's delay line under
its gate), or run alongside I2sToneOutput's writer task (the whisper build does not start it).
*/
class DuplexStream {
public:
    explicit DuplexStream(AudioSourceI2S& port)
        : _port(port) {}

    // Allocates the record (recordFrames int16) and fills the TX queue with silence. False without TX or heap.
    bool begin(size_t recordFrames);
    void setGain(float gain) { _gain = gain; }

    // The output for the mic block that started at rxIndex: its n samples (the mic's int16 scale) at the gain.
    void writeBlock(uint64_t rxIndex, const int16_t* samples, size_t n);

    // What was written for mic index rxIndex (int16 of the amp's full scale); false when never written or overwritten.
    bool played(uint64_t rxIndex, int16_t& out) const;
    uint64_t recordEnd() const { return _end; }
    size_t recordFrames() const { return _frames; }
    uint32_t shortWrites() const { return _shortWrites; }

    // The TX queue's depth in frames (the silence written at begin): the bulk of the speaker's delay behind the mic.
    static constexpr size_t kQueueFrames = static_cast<size_t>(I2S_DMA_BUF_COUNT / 2) * I2S_DMA_BUF_LEN;
    uint32_t refills() const { return _refills; }

private:
    static constexpr size_t kMaxBlock = 512;
    AudioSourceI2S& _port;
    std::unique_ptr<int16_t[]> _record;
    std::unique_ptr<bool[]> _written;
    size_t _frames = 0;
    uint64_t _end = 0;
    float _gain = 1.0f;
    uint32_t _shortWrites = 0;
    uint32_t _refills = 0;
    void writeSilence(size_t frames);
    int32_t _out[2 * kMaxBlock] = {};
};
