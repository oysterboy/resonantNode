"""A whisper corpus session with one voice (ECHO-005, step 26 of the whisper corpus): one take at a time on the
board's own mic, each started by the operator's go, so the voice is briefed for that take only.

  python tools/bench/whispersession.py --port COM3 --voice anna
  python tools/bench/whispersession.py --port COM3 --plan room          # the room's noises, once per day
  python tools/bench/whispersession.py --list                           # the takes, without a board

Per take the screen shows what to tell the voice and where they stand; Enter starts it. Three short beeps from this
PC count in, the third one higher: the voice starts after it. The recording runs the take's seconds, a long low beep
ends it. Then the level (RMS and peak against the room take) and Enter to keep, r to redo, q to stop. A redo
overwrites the take. Stopped half way? --from <take> goes on where it stopped (the board restarts when its port opens).

Each take: <out-dir>/<voice>_<nn>_<section>.wav, one row in <out-dir>/labels.csv (written after every take), the
console in <out-dir>/session_log.txt. The board: the echo or whisper firmware (REC), its playback switched off live
at the start. The other nodes in the room must be quiet: --gateway COMx mutes them all by radio first.

Needs pyserial; the beeps need Windows (winsound), elsewhere the count-in is printed only.
"""
import argparse
import csv
import datetime
import math
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import longrec  # noqa: E402

# One take: (section, kind, distance in m or None, seconds, what to tell the voice).
# Whispers: soft = barely audible to someone beside them; normal = a whisper to someone at the table; strained = a
# stage whisper meant to carry across a room, still without voice. Free content: talking keeps it natural, reading
# makes it flat.
VOICE_PLAN = [
    ("room_before", "room", None, 20,
     "Nobody speaks, nobody moves for 20 seconds. The room before the voice."),
    ("whisper_normal_05m", "whisper", 0.5, 30,
     "Sit or stand at the half-metre mark. Whisper normally, as to someone beside you: tell how you got here today. "
     "Keep going until the low beep."),
    ("whisper_soft_05m", "whisper", 0.5, 30,
     "Same place. Whisper very softly, barely audible: what you had for breakfast, what you see in the room."),
    ("whisper_strained_05m", "whisper", 0.5, 30,
     "Same place. A stage whisper: strong, meant to carry across a room, but still no voice. Any topic."),
    ("whisper_normal_1m", "whisper", 1.0, 30,
     "Move to the one-metre mark. Whisper normally again: tell about a place you like."),
    ("whisper_soft_1m", "whisper", 1.0, 30,
     "Same place, very soft whisper: describe your way home."),
    ("whisper_strained_1m", "whisper", 1.0, 30,
     "Same place, the stage whisper: any topic, no voice."),
    ("whisper_normal_2m", "whisper", 2.0, 30,
     "Move to the two-metre mark. Normal whisper: tell about your week."),
    ("whisper_soft_2m", "whisper", 2.0, 30,
     "Same place, very soft whisper: anything."),
    ("whisper_strained_2m", "whisper", 2.0, 30,
     "Same place, the stage whisper: anything."),
    ("whisper_words_1m", "whisper", 1.0, 30,
     "Back to one metre. Whisper single words with a pause of two or three seconds between them: names, colours, "
     "numbers."),
    ("speech_quiet_1m", "speech", 1.0, 30,
     "One metre. Speak with your voice now, quietly, as in a library: tell about a film or a book."),
    ("speech_normal_1m", "speech", 1.0, 30,
     "One metre. Speak normally, as at a table: anything."),
    ("breath_1m", "breath", 1.0, 20,
     "One metre. No words: breathe audibly, sigh, blow out slowly, a few times with pauses."),
    ("throat_1m", "throat", 1.0, 20,
     "One metre. No words: clear your throat, a cough, a hum, a short laugh, with pauses."),
    ("room_after", "room", None, 20,
     "Nobody speaks, nobody moves for 20 seconds. The room after the voice."),
]

# The room's own noises, once per recording day (the voice is the operator or nobody).
ROOM_PLAN = [
    ("room_alone", "room", None, 60, "Nobody in the room moves or speaks for a minute."),
    ("typing_1m", "noise", 1.0, 30, "Type on a keyboard at the one-metre mark, normal pace."),
    ("paper_1m", "noise", 1.0, 30, "At one metre: leaf through paper, crumple a sheet, fold it."),
    ("clothes_1m", "noise", 1.0, 30, "At one metre: a jacket on and off, sleeves rubbing, a zip."),
    ("chair_1m", "noise", 1.0, 30, "At one metre: sit down, shift, stand up, roll or move the chair."),
    ("steps_2m", "noise", 2.0, 30, "Walk past at two metres and back, a few times, normal shoes."),
]

PLANS = {"voice": VOICE_PLAN, "room": ROOM_PLAN}

FIELDS = ["date", "voice", "take", "section", "kind", "distance_m", "secs", "file", "filled_pct", "bad_frames",
          "rms", "peak", "time", "board"]


def beep(hz, ms):
    try:
        import winsound

        winsound.Beep(hz, ms)
    except (ImportError, RuntimeError):
        time.sleep(ms / 1000.0)


def level(pcm):
    """RMS and peak of int16 mono PCM."""
    n = len(pcm) // 2
    if n == 0:
        return 0.0, 0
    s = struct.unpack("<%dh" % n, pcm)
    return math.sqrt(sum(x * x for x in s) / n), max(abs(x) for x in s)


def ask(prompt, choices):
    while True:
        a = input(prompt).strip().lower()
        if a in choices:
            return a
        print("  (%s)" % ", ".join(c if c else "Enter" for c in choices))


class Log:
    def __init__(self, path):
        self.f = open(path, "a", encoding="utf-8")

    def __call__(self, text):
        print(text, flush=True)
        self.f.write(text + "\n")
        self.f.flush()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="the recording board's console (echo or whisper firmware)")
    ap.add_argument("--voice", help="a short name for the voice, used in the file names (e.g. anna)")
    ap.add_argument("--plan", choices=sorted(PLANS), default="voice")
    ap.add_argument("--out-dir", help="default bench/sessions/<today>-whisper-corpus")
    ap.add_argument("--from", dest="start", type=int, default=1, help="the take to start at (after a stop)")
    ap.add_argument("--boot-wait", type=float, default=12.0,
                    help="seconds after opening the port: the board restarts when its port opens")
    ap.add_argument("--gateway", help="the radio gateway's port: mutes every node by radio before the first take")
    ap.add_argument("--list", action="store_true", help="print the plan and exit")
    ap.add_argument("--dry-run", action="store_true", help="no board: 1 s of silence per take, to try the flow")
    A = ap.parse_args()

    plan = PLANS[A.plan]
    if A.list:
        for i, (section, kind, dist, secs, brief) in enumerate(plan, 1):
            where = "%.1f m" % dist if dist else "-"
            print("%2d  %-22s %-8s %-6s %3d s  %s" % (i, section, kind, where, secs, brief))
        print("total %.1f min of recording" % (sum(p[3] for p in plan) / 60.0))
        return
    voice = A.voice or ("room" if A.plan == "room" else None)
    if not voice or not voice.replace("-", "").replace("_", "").isalnum():
        sys.exit("--voice <name>: letters, digits, - or _ (it goes into the file names)")
    if not A.port and not A.dry_run:
        sys.exit("--port COMx (or --dry-run)")

    today = datetime.date.today().isoformat()
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    out_dir = A.out_dir or os.path.join(root, "bench", "sessions", today + "-whisper-corpus")
    os.makedirs(out_dir, exist_ok=True)
    log = Log(os.path.join(out_dir, "session_log.txt"))
    labels_path = os.path.join(out_dir, "labels.csv")
    rows = []
    if os.path.exists(labels_path):
        with open(labels_path, newline="", encoding="utf-8") as f:
            rows = list(csv.DictReader(f))

    def save_labels():
        with open(labels_path, "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=FIELDS)
            w.writeheader()
            w.writerows(rows)

    log("\n=== %s  voice=%s  plan=%s  from take %d  out=%s" % (datetime.datetime.now().isoformat(timespec="seconds"),
                                                              voice, A.plan, A.start, out_dir))
    if A.gateway:
        gw = longrec.open_port(A.gateway)
        time.sleep(0.5)
        reply = longrec.console(gw, "NET * PARAM SET output.silent 1", wait=3.0, show=False)
        gw.close()
        muted = sorted({x.split("from=")[1].split()[0] for x in reply.splitlines()
                        if "from=" in x and "status=ok" in x})
        log("nodes muted by radio: %s" % (", ".join(muted) or "none answered"))

    board = "dry-run"
    ser = None
    if not A.dry_run:
        ser = longrec.open_port(A.port)
        log("waiting %.0f s for the board's boot ..." % A.boot_wait)
        board = longrec.wait_boot(ser, A.boot_wait)
        log(board)
        for cmd in ("PARAM SET whisper.enabled 0", "PARAM SET echo.enabled 0"):
            reply = longrec.console(ser, cmd, show=False)
            if "status=ok" in reply:
                log("playback off: " + cmd)

    room_rms = None
    for r in rows:
        if r["voice"] == voice and r["kind"] == "room" and r["rms"]:
            room_rms = float(r["rms"])

    i = A.start
    while i <= len(plan):
        section, kind, dist, secs, brief = plan[i - 1]
        where = "at the %.1f m mark" % dist if dist else "the voice stays quiet"
        print("\n" + "-" * 78)
        print("Take %d of %d: %s  (%d s, %s)" % (i, len(plan), section, secs, where))
        print("\n  Tell them: %s\n" % brief)
        a = ask("  Enter = go (count-in, then %d s)   s = skip   q = stop > " % secs, ["", "s", "q"])
        if a == "q":
            break
        if a == "s":
            log("take %d %s skipped" % (i, section))
            i += 1
            continue

        for hz in (880, 880, 1320):
            beep(hz, 150)
            time.sleep(0.85 if hz == 880 else 0.0)
        print("  >>> recording %d s" % secs, flush=True)
        t0 = datetime.datetime.now().isoformat(timespec="seconds")
        if A.dry_run:
            time.sleep(1.0)
            rate, pcm, filled, bad, ended = 24000, bytes(48000), 24000, 0, "REC end (dry run)"
        else:
            try:
                rate, pcm, filled, bad, ended = longrec.record(ser, secs, progress=False)
            except RuntimeError as e:
                log("take %d %s: %s" % (i, section, e))
                if ask("  r = try again   q = stop > ", ["r", "q"]) == "q":
                    break
                continue
        beep(440, 700)

        name = "%s_%02d_%s.wav" % (voice, i, section)
        longrec.write_wav(os.path.join(out_dir, name), rate, pcm)
        total = len(pcm) // 2
        rms, peak = level(pcm)
        filled_pct = 100.0 * filled / total if total else 0.0
        vs_room = "  (%.1f dB over the room)" % (20 * math.log10(rms / room_rms)) if room_rms and rms > 0 else ""
        log("take %d %s: %s, %.1f %% filled, %d bad, rms %.0f%s, peak %d, %s"
            % (i, section, name, filled_pct, bad, rms, vs_room, peak, ended or "no REC end"))
        if filled_pct < 99.0:
            print("  !! more than 1 % of the samples missing: redo it")

        rows[:] = [r for r in rows if r["file"] != name]
        rows.append({"date": today, "voice": voice, "take": i, "section": section, "kind": kind,
                     "distance_m": dist if dist else "", "secs": secs, "file": name,
                     "filled_pct": "%.2f" % filled_pct, "bad_frames": bad, "rms": "%.1f" % rms, "peak": peak,
                     "time": t0, "board": board})
        save_labels()
        if kind == "room":
            room_rms = rms

        a = ask("  Enter = keep, next take   r = redo   q = stop > ", ["", "r", "q"])
        if a == "r":
            log("take %d %s: redo" % (i, section))
            continue
        if a == "q":
            i += 1
            break
        i += 1

    if ser:
        ser.close()
    if i <= len(plan):
        log("stopped before take %d; go on with --from %d" % (i, i))
    else:
        log("session done: %d takes in %s" % (len(plan), labels_path))


if __name__ == "__main__":
    main()
