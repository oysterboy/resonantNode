#pragma once

#include "../../io/AudioSignal.h"
#include "SignalCandidate.h"
#include "ScalarSignalEmitter.h"

namespace detection {

// Roadmap adapter for the AMP scalar path.
// Scalar transient mechanics now live in ScalarSignalEmitter / ScalarTransientDetector.
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
