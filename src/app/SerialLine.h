#pragma once

#include <stddef.h>

/*
SerialLine

The line reader every serial port in every mode uses (step 11e item 8;
decisions/2026-10-25-analyzer-params-on-registry-and-line-policy.md), and
its byte filter.

Byte filter: a reset or reflash of one board leaves its TX pin floating or
low for a moment; the other board reads that as a few junk bytes (0x00,
0xFF, ...) with no newline after them. They used to sit in front of the
next command in the line buffer, so "MODE REMOTE" arrived as
"<junk>MODE REMOTE" and was ignored (D-AMP bench, 2026-10-09). The protocols
are plain ASCII, so the reader drops CR and anything non-printable; '\n'
still ends a line.

Overflow policy: a line longer than the buffer is dropped whole and reported
(tooLong()), never run cut: a cut "PARAM SET x 1200" is "PARAM SET x 12"
(issue #31 A3 on the Node; the Analyzer and the Emitter ran cut lines until
step 11e).
*/
namespace serial_line {

// The line bounds, stated once (review D2.10, 2026-10-26): a command line on a USB console (Node, Analyzer) and
// the PARAM handler's copy of it; a line on the Emitter link (both Emitter ports and the Analyzer's readers of it,
// 95 because the Emitter's commands are short and its buffers small). net::kMaxText (233) is the radio's own
// wire limit, checked separately before a remote line is run.
constexpr size_t kCommandLineMax = 255;
constexpr size_t kEmitterLineMax = 95;

inline bool dropByte(char c) {
    const unsigned char u = static_cast<unsigned char>(c);
    return c != '\n' && (u < 0x20 || u > 0x7E);
}

// Capacity is the longest line in bytes, without the terminator.
template <size_t Capacity>
class LineReader {
public:
    static constexpr size_t kCapacity = Capacity;

    // Feeds one byte. True when a line ended: line() holds it (possibly
    // empty), or tooLong() says it overflowed and was dropped whole. The
    // byte after a finished line starts the next one.
    bool feed(char c) {
        if (_ready) {
            reset();
        }
        if (dropByte(c)) {
            return false;
        }
        if (c != '\n') {
            if (_length < Capacity) {
                _buffer[_length++] = c;
            } else {
                _tooLong = true;
            }
            return false;
        }
        if (_tooLong) {
            _length = 0;
        }
        _buffer[_length] = '\0';
        _ready = true;
        return true;
    }

    // The finished line, valid after feed() returned true until the next feed().
    const char* line() const { return _buffer; }
    size_t length() const { return _length; }
    bool tooLong() const { return _tooLong; }

    void reset() {
        _length = 0;
        _tooLong = false;
        _ready = false;
        _buffer[0] = '\0';
    }

private:
    char _buffer[Capacity + 1] = {};
    size_t _length = 0;
    bool _tooLong = false;
    bool _ready = false;
};

// One poll of a port, the loop every console runs (step 11j): reads what the port holds; a finished line goes to
// handle(line), an empty one is skipped, an overflowed one is refused with `tooLongReply` and the capacity
// ("ERR line_too_long max=255"). Port: available() / read(); Out: print() / println() (Serial, serialOut()).
template <size_t Capacity, typename Port, typename Out, typename Handler>
void pollLines(Port& port, LineReader<Capacity>& reader, Out& out, const char* tooLongReply, Handler&& handle) {
    while (port.available() > 0) {
        if (!reader.feed(static_cast<char>(port.read()))) {
            continue;
        }
        if (reader.tooLong()) {
            out.print(tooLongReply);
            out.println(static_cast<unsigned long>(Capacity));
        } else if (reader.length() > 0) {
            handle(reader.line());
        }
    }
}

} // namespace serial_line
