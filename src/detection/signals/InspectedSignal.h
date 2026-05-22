#pragma once

#include "../inspector/InspectorTypes.h"
#include "SignalCandidate.h"

namespace detection {

/*
InspectedSignal

SignalCandidate plus SignalInspector decision and added evidence.
Owns candidateAccepted and signal-stage rejection reason.
*/
enum class SignalDecision {
    None,
    Accepted,
    Rejected
};

enum class SignalRejectReason {
    None,
    TooShort,
    TooLong,
    TooWeak,
    BelowThreshold,
    DuplicateRisk,
    Cooldown,
    MissingFrequencyEvidence,
    MissingAmpSupport,
    InvalidTiming,
    UnsupportedKind,
    Unknown
};

struct InspectedSignal {
    SignalCandidate signal = {};
    SignalDecision decision = SignalDecision::None;

    bool accepted = false;
    bool rejected = false;

    SignalRejectReason rejectReason = SignalRejectReason::None;
    unsigned long durationMs = 0;
    float strength = 0.0f;
    float confidence = 0.0f;
    float signalConfidence = 0.0f;
    float frequencyConfidence = 0.0f;
    AmpSupportLevel ampSupport = AmpSupportLevel::Unknown;
    AmpWindowEvidence ampWindow = {};
    bool duplicateRisk = false;
    float duplicateRiskScore = 0.0f;
};

} // namespace detection
