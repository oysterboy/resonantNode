#pragma once

#include <stdint.h>

/*
WhisperGate (ECHO-002)

Whispernet's gate rule over the verdicts on a running phrase: when the delay line passes the mic. Detection judges the
open phrase every ~100 ms (EnvelopeRuntime's provisional verdicts); this class turns those verdicts into the gate's
openings and closings:

- The first passing verdict of a phrase asks for a decision (Behavior: answer this phrase or not); an answered phrase
  opens the gate from its start, at most the delay line's length back (what is still unplayed).
- A failing verdict closes it; a later passing one in the same answered phrase opens it again from the closing.
- A phrase once too loud (its level above the loud bound) keeps the gate shut to its end: speech has quiet
  half-seconds that read inside the window, and a growing own replay is the howl the bound guards against.
- The phrase's end (its final verdict, or the detector dropping it) closes the gate.

The shell and native-echoreplay run this one class, so the PC replay gates as the board does. Pure, host-tested
(test_whisper_gate).

Owns: the phrase being followed, its answer and loudness latch, the gate's state and its last closing.
Does not: decide whether to answer (Behavior), judge a phrase (the inspector and makeVerdict), or move samples
(GatedDelayLine).
*/
class WhisperGate {
public:
    enum class Event : uint8_t {
        None,
        Ask,    // a new phrase passed: decide(), then the gate may open
        Open,   // the gate opens; fromMs is the first ms to pass
        Close,  // the gate shuts at the time given
    };
    struct Step {
        Step() = default;
        Step(Event e, unsigned long from)
            : event(e),
              fromMs(from) {}  // the ESP toolchain's C++11: no aggregate init
        Event event = Event::None;
        unsigned long fromMs = 0;  // Open: pass from here; Close: shut here
    };

    void setDelayMs(unsigned long delayMs) { _delayMs = delayMs; }
    unsigned long delayMs() const { return _delayMs; }
    void reset() {
        const unsigned long delayMs = _delayMs;
        *this = WhisperGate{};
        _delayMs = delayMs;
    }

    // A provisional verdict on the running phrase that started at phraseStartMs: valid (the plan passed) and tooLoud
    // (its level class was None from above the bound) at nowMs.
    Step onVerdict(unsigned long phraseStartMs, bool valid, bool tooLoud, unsigned long nowMs) {
        if (!_following || phraseStartMs != _phraseStartMs) {
            Step closed = closeAt(nowMs);  // a new phrase: whatever was open belonged to the last one
            _following = true;
            _phraseStartMs = phraseStartMs;
            _tooLoud = false;
            _decided = false;
            _answered = false;
            if (closed.event == Event::Close) {
                return closed;
            }
        }
        _tooLoud = _tooLoud || tooLoud;
        const bool pass = valid && !_tooLoud;
        if (!pass) {
            return closeAt(nowMs);
        }
        if (!_decided) {
            _askAtMs = nowMs;
            return Step{Event::Ask, 0};
        }
        return _answered ? openAt(nowMs) : Step{};
    }

    // Behavior's answer for the phrase that asked. An answered phrase opens the gate now.
    Step decide(bool answer) {
        if (!_following || _decided) {
            return Step{};
        }
        _decided = true;
        _answered = answer;
        return answer ? openAt(_askAtMs) : Step{};
    }

    // The phrase's end (its final verdict, or the detector dropped it) at endMs.
    Step onPhraseEnd(unsigned long endMs) {
        const Step closed = closeAt(endMs);
        _following = false;
        return closed;
    }

    bool open() const { return _open; }
    bool following() const { return _following; }
    unsigned long phraseStartMs() const { return _phraseStartMs; }

private:
    Step openAt(unsigned long nowMs) {
        if (_open) {
            return Step{};
        }
        unsigned long from = nowMs > _delayMs ? nowMs - _delayMs : 0UL;  // older is already played
        from = static_cast<long>(_phraseStartMs - from) > 0 ? _phraseStartMs : from;
        from = _closedOnce && static_cast<long>(_lastCloseMs - from) > 0 ? _lastCloseMs : from;
        _open = true;
        return Step{Event::Open, from};
    }
    Step closeAt(unsigned long nowMs) {
        if (!_open) {
            return Step{};
        }
        _open = false;
        _closedOnce = true;
        _lastCloseMs = nowMs;
        return Step{Event::Close, nowMs};
    }

    unsigned long _delayMs = 2000;
    bool _following = false;
    unsigned long _phraseStartMs = 0;
    bool _tooLoud = false;
    bool _decided = false;
    bool _answered = false;
    unsigned long _askAtMs = 0;
    bool _open = false;
    bool _closedOnce = false;
    unsigned long _lastCloseMs = 0;
};
