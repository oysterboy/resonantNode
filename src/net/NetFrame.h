#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

/*
NetFrame (PAR-013, step 11a): the radio frame of the Node's remote command path.

One ESP-NOW frame (<= 250 bytes) carries one command line or one part of a
reply. A command goes to every node (broadcast) with a target: "*" or a node
name (E1..E5, app/NodeRoster.h). The text is the same command line serial
takes (PARAM SET ..., PARAM SAVE, ...), so radio and serial share one command
path, one validation and one store (PAR-013: no transport-specific store).
Pure: no Arduino, no radio; host-tested in test/test_net_frame.

Wire layout (little endian):
  0  'R' 'N'      magic
  2  version      kVersion
  3  type         FrameType
  4  netId        frames of another network are ignored (build flag NET_ID)
  5  seq          uint16, per sender; a receiver drops repeats (SeqFilter)
  7  part         reply part number, 0 for a command
  8  flags        bit 0: last part of the reply
  9  target[8]    command: "*" or a node name; reply: the replying node's name
 17  text         up to kMaxText bytes, no terminator on the wire
*/
namespace net {

constexpr uint8_t kMagic0 = 'R';
constexpr uint8_t kMagic1 = 'N';
constexpr uint8_t kVersion = 1;
constexpr size_t kMaxFrameBytes = 250;  // ESP_NOW_MAX_DATA_LEN
constexpr size_t kTargetLen = 8;
constexpr size_t kHeaderBytes = 9 + kTargetLen;
constexpr size_t kMaxText = kMaxFrameBytes - kHeaderBytes;  // 233
constexpr uint8_t kFlagLast = 0x01;

enum class FrameType : uint8_t {
    Command = 1,
    Reply = 2,
};

struct Frame {
    FrameType type = FrameType::Command;
    uint8_t netId = 0;
    uint16_t seq = 0;
    uint8_t part = 0;
    bool last = true;
    char target[kTargetLen + 1] = {};
    char text[kMaxText + 1] = {};  // NUL-terminated in memory
    size_t textLen = 0;
};

inline void setTarget(Frame& f, const char* target) {
    memset(f.target, 0, sizeof(f.target));
    if (target != nullptr) {
        strncpy(f.target, target, kTargetLen);
    }
}

// Copies at most kMaxText bytes; returns how many were taken.
inline size_t setText(Frame& f, const char* text, size_t len) {
    if (len > kMaxText) {
        len = kMaxText;
    }
    memcpy(f.text, text, len);
    f.text[len] = '\0';
    f.textLen = len;
    return len;
}

// Returns the encoded length, 0 if out is too small.
inline size_t encode(const Frame& f, uint8_t* out, size_t capacity) {
    const size_t len = kHeaderBytes + f.textLen;
    if (f.textLen > kMaxText || capacity < len) {
        return 0;
    }
    out[0] = kMagic0;
    out[1] = kMagic1;
    out[2] = kVersion;
    out[3] = static_cast<uint8_t>(f.type);
    out[4] = f.netId;
    out[5] = static_cast<uint8_t>(f.seq & 0xFF);
    out[6] = static_cast<uint8_t>(f.seq >> 8);
    out[7] = f.part;
    out[8] = f.last ? kFlagLast : 0;
    memcpy(out + 9, f.target, kTargetLen);
    memcpy(out + kHeaderBytes, f.text, f.textLen);
    return len;
}

// False for anything that is not a well-formed frame of this version.
inline bool decode(const uint8_t* in, size_t len, Frame& f) {
    if (in == nullptr || len < kHeaderBytes || len > kMaxFrameBytes) {
        return false;
    }
    if (in[0] != kMagic0 || in[1] != kMagic1 || in[2] != kVersion) {
        return false;
    }
    if (in[3] != static_cast<uint8_t>(FrameType::Command) && in[3] != static_cast<uint8_t>(FrameType::Reply)) {
        return false;
    }
    f.type = static_cast<FrameType>(in[3]);
    f.netId = in[4];
    f.seq = static_cast<uint16_t>(in[5] | (in[6] << 8));
    f.part = in[7];
    f.last = (in[8] & kFlagLast) != 0;
    memcpy(f.target, in + 9, kTargetLen);
    f.target[kTargetLen] = '\0';
    f.textLen = len - kHeaderBytes;
    memcpy(f.text, in + kHeaderBytes, f.textLen);
    f.text[f.textLen] = '\0';
    // The text is a command line: no embedded NUL or newline.
    for (size_t i = 0; i < f.textLen; ++i) {
        if (f.text[i] == '\0' || f.text[i] == '\n' || f.text[i] == '\r') {
            return false;
        }
    }
    return true;
}

// "*" matches every node; otherwise the node name, case-insensitive.
inline bool targetMatches(const char* target, const char* myName) {
    if (target == nullptr || target[0] == '\0') {
        return false;
    }
    if (strcmp(target, "*") == 0) {
        return true;
    }
    return myName != nullptr && strcasecmp(target, myName) == 0;
}

// A reply is a byte stream cut into frames: a line end travels as kLineEnd, and
// a long line (RB STATUS, ~2 KB) runs on into the next frame. The gateway joins
// the stream per sender and prints whole lines (LineJoiner).
constexpr char kLineEnd = '\x1F';

template <size_t Capacity>
class LineJoiner {
public:
    // Feeds one frame's text; calls emit(line, len) for every whole line, and
    // for the rest when `last`.
    template <typename Emit>
    void feed(const char* text, size_t len, bool last, Emit emit) {
        for (size_t i = 0; i < len; ++i) {
            if (text[i] == kLineEnd) {
                emit(_buf, _len);
                _len = 0;
            } else if (_len < Capacity) {
                _buf[_len++] = text[i];
            }
        }
        if (last && _len > 0) {
            emit(_buf, _len);
            _len = 0;
        }
    }
    void reset() { _len = 0; }
    // A frame went missing: the partial line so far, as it is (not glued to the next frame's text).
    template <typename Emit>
    void flush(Emit emit) {
        if (_len > 0) {
            emit(_buf, _len);
            _len = 0;
        }
    }

private:
    char _buf[Capacity];
    size_t _len = 0;
};

// Drops a repeated (sender, seq): a command is broadcast several times for
// reliability and must run once. Remembers the last seq of kSenders senders.
class SeqFilter {
public:
    static constexpr size_t kSenders = 8;

    bool accept(const uint8_t mac[6], uint16_t seq) {
        for (size_t i = 0; i < kSenders; ++i) {
            if (_used[i] && memcmp(_mac[i], mac, 6) == 0) {
                if (_seq[i] == seq) {
                    return false;
                }
                _seq[i] = seq;
                return true;
            }
        }
        const size_t slot = _next;
        _next = (_next + 1) % kSenders;
        memcpy(_mac[slot], mac, 6);
        _seq[slot] = seq;
        _used[slot] = true;
        return true;
    }

private:
    uint8_t _mac[kSenders][6] = {};
    uint16_t _seq[kSenders] = {};
    bool _used[kSenders] = {};
    size_t _next = 0;
};

}  // namespace net
