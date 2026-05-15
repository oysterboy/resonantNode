#pragma once

#include "SignalCandidate.h"

struct AudioSignalFrame;
class AmpCandidateBuilder;
class AmpTransientDetector;

namespace detection {

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
