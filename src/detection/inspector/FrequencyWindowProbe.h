#pragma once

#include "../../io/AudioSignal.h"
#include "../inspector/InspectorTypes.h"

namespace detection {

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
