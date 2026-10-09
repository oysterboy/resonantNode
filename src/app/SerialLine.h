#pragma once

/*
SerialLine

Byte filter for the line-based Analyzer <-> Emitter protocol on Serial2.

A reset or reflash of one board leaves its TX pin floating or low for a
moment; the other board reads that as a few junk bytes (0x00, 0xFF, ...)
with no newline after them. They used to sit in front of the next command
in the line buffer, so "MODE REMOTE" arrived as "<junk>MODE REMOTE" and
was ignored (D-AMP bench, 2026-10-09). The protocol is plain ASCII, so the
readers drop CR and anything non-printable; '\n' still ends a line.
*/
namespace serial_line {

inline bool dropByte(char c) {
    const unsigned char u = static_cast<unsigned char>(c);
    return c != '\n' && (u < 0x20 || u > 0x7E);
}

} // namespace serial_line
