"""A long recording from a board's own mic over USB: `REC <secs>` on the echo room or Whispernet firmware streams the
mic as framed int16 PCM at 921600 baud (src/audio/LongRecorder.h); this reads it into a 16-bit mono WAV.

  python tools/bench/longrec.py --port COM5 --secs 120 --out bench/sessions/<folder>/anna_whisper_05m.wav

The board does the baud switch itself: the "REC start" line arrives at 115200, the port goes to 921600 after it, the
"REC end" line brings it back. Run the board with its own playback off (echo.enabled 0 / whisper.enabled 0), the
stream is the raw mic. Frames that arrive damaged or not at all (the position jumps) are filled with silence and
counted. Ctrl+C sends REC STOP and keeps what came. Needs pyserial.
"""
import argparse
import struct
import sys
import time
import wave

MAGIC = bytes([0xA5, 0x5A])
NL = bytes([10])


def parse(buf, pcm, total):
    """Frames out of `buf` into `pcm`: A5 5A pos(4) n(2) samples(2n) sum(2). Returns the unparsed rest, the samples
    taken, the bad frames and the "REC end" line when it passed."""
    filled, bad, ended = 0, 0, None
    buf = bytearray(buf)
    while True:
        i = buf.find(MAGIC)
        if i < 0:
            j = buf.find(b"REC end")
            if j >= 0 and NL in buf[j:]:
                ended = bytes(buf[j:]).split(NL)[0].decode(errors="replace")
            return buf[-300:], filled, bad, ended
        if i > 0:
            j = buf.find(b"REC end", 0, i)
            if j >= 0 and NL in buf[j:i]:
                ended = bytes(buf[j:i]).split(NL)[0].decode(errors="replace")
            del buf[:i]
        if len(buf) < 8:
            return buf, filled, bad, ended
        pos, n = struct.unpack_from("<IH", buf, 2)
        size = 10 + 2 * n
        if n == 0 or n > 120 or pos + n > total:
            del buf[:2]
            bad += 1
            continue
        if len(buf) < size:
            return buf, filled, bad, ended
        if (sum(buf[:size - 2]) & 0xFFFF) != struct.unpack_from("<H", buf, size - 2)[0]:
            del buf[:2]
            bad += 1
            continue
        pcm[2 * pos:2 * (pos + n)] = buf[8:8 + 2 * n]
        filled += n
        del buf[:size]


def main():
    import serial

    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--secs", type=int, required=True, help="1..600")
    ap.add_argument("--out", required=True, help="the WAV to write")
    ap.add_argument("--baud", type=int, default=115200, help="the console's baud before and after the stream")
    A = ap.parse_args()

    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout = A.port, A.baud, 0.05
    ser.dtr = False
    ser.rts = False
    ser.open()
    ser.reset_input_buffer()
    ser.write(("REC %d\n" % A.secs).encode())

    # The start line, then the baud.
    text, t_end = b"", time.time() + 5
    while time.time() < t_end:
        text += ser.read(256)
        if b"baud=921600" in text:
            break
        if b"REC ERR" in text:
            sys.exit(text.decode(errors="replace").strip())
    else:
        sys.exit("no REC start from the board (the echo or whisper firmware?): %r" % text[-200:])
    line = [x for x in text.decode(errors="replace").splitlines() if "REC start" in x][0]
    rate = int(line.split("rate=")[1].split()[0])
    ser.baudrate = 921600
    ser.reset_input_buffer()
    print(line.strip(), flush=True)

    total = rate * A.secs
    pcm = bytearray(total * 2)
    filled = bad = 0
    buf = bytearray()
    last = time.time()
    ended = None
    shown = 0
    try:
        while time.time() - last < 8:
            data = ser.read(8192)
            if data:
                last = time.time()
                buf += data
            buf, got, nbad, end_line = parse(buf, pcm, total)
            filled += got
            bad += nbad
            ended = end_line or ended
            if ended:
                break
            if time.time() - shown > 10:
                shown = time.time()
                print("%d / %d samples" % (filled, total), flush=True)
    except KeyboardInterrupt:
        ser.baudrate = 921600
        ser.write(b"REC STOP\n")
        time.sleep(1.0)
        print("stopped")

    ser.baudrate = A.baud
    ser.close()
    with wave.open(A.out, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(bytes(pcm))
    print("%s: %d of %d samples received (%.1f %% filled), %d bad frames; board: %s"
          % (A.out, filled, total, 100.0 * filled / total, bad, ended or "no REC end seen"))


if __name__ == "__main__":
    main()
