#pragma once

#include "SignalCandidate.h"

struct AudioSignalFrame;
class AmpCandidateBuilder;
class AmpTransientDetector;

namespace detection {

// Roadmap adapter for the AMP scalar path.
// Scalar transient mechanics already live in ScalarTransientDetector via AmpTransientDetector.
class AmpSignalEmitter {
public:
    AmpSignalEmitter();

    void reset();

    void observeFrame(
        const AudioSignalFrame& frame,
        AmpTransientDetector& detector,
        AmpCandidateBuilder& builder
    );

    bool popSignalCandidate(SignalCandidate& out);

private:
    bool _hasPending = false;
    SignalCandidate _pending = {};
};

} // namespace detection
