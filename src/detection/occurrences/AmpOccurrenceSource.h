#pragma once

#include "../../io/AudioSignal.h"
#include "Occurrence.h"
#include "ScalarOccurrenceSource.h"

namespace detection {

/*
AmpOccurrenceSource

Owns the AMP-side occurrence candidate path.
Wraps ScalarOccurrenceSource to produce AMP transient candidates from AudioSignalFrame input.
Does not decide pattern meaning or behavior.
*/
class AmpOccurrenceSource {
public:
    AmpOccurrenceSource();

    void reset();

    void observeFrame(const AudioSignalFrame& frame);

    bool popOccurrence(Occurrence& out);

private:
    static void fillAmpCandidate(Occurrence& candidate, const AudioSignalFrame& frame);

    bool _hasPending = false;
    ScalarOccurrenceSource _scalarEmitter = {};
    Occurrence _pending = {};
};

} // namespace detection

