#pragma once

#include "../../io/AudioSignal.h"
#include "../inspector/InspectorTypes.h"

namespace detection {

/*
FrequencyWindowProbe

Retrospective helper for measuring frequency evidence around a candidate window.
Produces FrequencyEvidence only; it does not create SignalCandidate or PatternResult.
*/
bool measureCandidateWindowFrequency(const AudioSignal& audioSignal,
                                     const DetectorCandidate& candidate,
                                     unsigned long sampleRateHz,
                                     unsigned long targetFrequencyHz,
                                     unsigned long observedAtMs,
                                     detection::FrequencyEvidence& out,
                                     unsigned long maxWindowMs = 100UL);

bool measureCandidateWindowFrequencyParityScan64(const AudioSignal& audioSignal,
                                                 const DetectorCandidate& candidate,
                                                 unsigned long sampleRateHz,
                                                 unsigned long targetFrequencyHz,
                                                 unsigned long observedAtMs,
                                                 FrequencyEvidence& out,
                                                 unsigned long windowSampleCount = 64UL);

} // namespace detection
