#pragma once

#include "patterns/PatternPayload.h"
#include "patterns/PatternNames.h"

/*
DetectionPipeline

Owns the lightweight compatibility layer between detector output and
behavior-level decisions.

Responsibilities:
- translate detector candidates from stream-extractor/detector stages into pattern candidates/results
- carry transient and frequency evidence through the pipeline
- classify pattern type and rejection reason strings for logging/debugging
- keep the compatibility aliases for legacy and analyzer paths

Does NOT:
- read audio directly
- own detector thresholds or tuning policy
- decide behavior timing or output actions

Roadmap v0.3 note:
- canonical pattern payloads now live in the dedicated pattern headers
- this header keeps the compatibility namespace, helper functions, and legacy aliases
*/
namespace DetectionPipeline {

using detection::AmpSupportClass;
using detection::FrequencyEvidence;
using detection::LocalityClass;
using detection::PatternCandidate;
using detection::PatternCandidateKind;
using detection::PatternReasonCode;
using detection::PatternRejectReason;
using detection::PatternResult;
using detection::PatternResultKind;
using detection::PatternSource;
using detection::PatternType;
using detection::TransientEvidence;

} // namespace DetectionPipeline
