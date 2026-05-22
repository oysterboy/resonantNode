#pragma once

#include "../../io/AudioSignal.h"
#include "SignalCandidate.h"
#include "ScalarSignalEmitter.h"

namespace detection {

/*
AmpSignalEmitter

Owns the AMP-side signal candidate path.
Wraps ScalarSignalEmitter to produce AMP transient candidates from AudioSignalFrame input.
Does not decide pattern meaning or behavior.
*/
class AmpSignalEmitter {
public:
    AmpSignalEmitter();

    void reset();

    void observeFrame(const AudioSignalFrame& frame);

    bool popSignalCandidate(SignalCandidate& out);

private:
    static void fillAmpCandidate(SignalCandidate& candidate, const AudioSignalFrame& frame);

    bool _hasPending = false;
    ScalarSignalEmitter _scalarEmitter = {};
    SignalCandidate _pending = {};
};

} // namespace detection
