#pragma once

#include "../io/AudioSignal.h"
#include "patterns/PatternPayload.h"

namespace DetectionPipeline {

using FrequencyEvidence = detection::FrequencyEvidence;

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
                                                 detection::FrequencyEvidence& out,
                                                 unsigned long windowSampleCount = 64UL);

} // namespace DetectionPipeline
