#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../../inspection/InspectorTypes.h"

/*
FrequencyMatchCriteria

Threshold parsing and evaluation helpers for frequency-match evidence.
This is occurrence/profile tuning support, not pattern matching.
*/
namespace FrequencyMatchCriteria {

struct Values {
    float attackScoreMin = 18000.0f;
    float releaseScoreMin = 12000.0f;
    float attackContrastMin = 50.0f;
    float releaseContrastMin = 50.0f;
};

struct ClassifierTuning {
    Values frequency = {};
};

enum class Reason {
    None,
    NoEvidence,
    AttackScoreTooLow,
    AttackContrastTooLow,
    AttackScoreAndContrastTooLow,
    ReleaseScoreTooLow,
    ReleaseContrastTooLow,
    ReleaseScoreAndContrastTooLow,
};

struct FrequencyMatchGateResult {
    bool evidenceOk = false;
    bool attackScoreOk = false;
    bool attackContrastOk = false;
    bool attackOk = false;
    bool releaseScoreOk = false;
    bool releaseContrastOk = false;
    bool releaseOk = false;
    bool matched = false;
    float score = 0.0f;
    float contrast = 0.0f;
    float attackScoreMin = 0.0f;
    float releaseScoreMin = 0.0f;
    float attackContrastMin = 0.0f;
    float releaseContrastMin = 0.0f;
    Reason attackReason = Reason::None;
    Reason releaseReason = Reason::None;
};

using Evaluation = FrequencyMatchGateResult;

// A finite number with nothing after it. On anything else the value is left
// unchanged: strtof(..., nullptr) turned "x" into 0 (the node then triggered
// on noise) and accepted "nan" (detection off) (issue #31 R2-4).
inline bool parseFiniteFloat(const char* text, float& out) {
    if (text == nullptr || *text == '\0') {
        return false;
    }
    char* end = nullptr;
    const float parsed = strtof(text, &end);
    if (end == text || *end != '\0' || isnan(parsed) || isinf(parsed)) {
        return false;
    }
    out = parsed;
    return true;
}

// true when the token is a frequency key with a valid value; false when the
// key is unknown or its value doesn't parse (the value is then unchanged).
inline bool parseToken(const char* token, Values& values) {
    if (token == nullptr) {
        return false;
    }
    if (strncmp(token, "freqAttackScore=", 16) == 0) {
        return parseFiniteFloat(token + 16, values.attackScoreMin);
    }
    if (strncmp(token, "freqReleaseScore=", 17) == 0) {
        return parseFiniteFloat(token + 17, values.releaseScoreMin);
    }
    if (strncmp(token, "freqAttackContrast=", 19) == 0) {
        return parseFiniteFloat(token + 19, values.attackContrastMin);
    }
    if (strncmp(token, "freqReleaseContrast=", 20) == 0) {
        return parseFiniteFloat(token + 20, values.releaseContrastMin);
    }
    if (strncmp(token, "freqScore=", 10) == 0) {
        return parseFiniteFloat(token + 10, values.attackScoreMin);
    }
    if (strncmp(token, "freqContrast=", 13) == 0) {
        return parseFiniteFloat(token + 13, values.attackContrastMin);
    }
    return false;
}

inline const char* reasonName(Reason reason) {
    switch (reason) {
        case Reason::None:
            return "none";
        case Reason::NoEvidence:
            return "no_frequency_evidence";
        case Reason::AttackScoreTooLow:
            return "freq_score_too_low";
        case Reason::AttackContrastTooLow:
            return "freq_contrast_too_low";
        case Reason::AttackScoreAndContrastTooLow:
            return "freq_score_and_contrast_too_low";
        case Reason::ReleaseScoreTooLow:
            return "freq_release_score_too_low";
        case Reason::ReleaseContrastTooLow:
            return "freq_release_contrast_too_low";
        case Reason::ReleaseScoreAndContrastTooLow:
            return "freq_release_score_and_contrast_too_low";
    }

    return "unknown";
}

inline FrequencyMatchGateResult evaluate(const detection::FrequencyBandMeasurementPacket& evidence, const Values& values) {
    FrequencyMatchGateResult out;
    out.evidenceOk = evidence.present;
    out.score = evidence.targetBandValue;
    out.contrast = evidence.targetBandContrastValue;
    out.attackScoreMin = values.attackScoreMin;
    out.releaseScoreMin = values.releaseScoreMin;
    out.attackContrastMin = values.attackContrastMin;
    out.releaseContrastMin = values.releaseContrastMin;

    out.attackScoreOk = evidence.targetBandValue >= values.attackScoreMin;
    out.attackContrastOk = evidence.targetBandContrastValue >= values.attackContrastMin;
    out.attackOk = out.evidenceOk && out.attackScoreOk;

    out.releaseScoreOk = evidence.targetBandValue >= values.releaseScoreMin;
    out.releaseContrastOk = evidence.targetBandContrastValue >= values.releaseContrastMin;
    out.releaseOk = out.evidenceOk && out.releaseScoreOk;

    if (!out.evidenceOk) {
        out.attackReason = Reason::NoEvidence;
        out.releaseReason = Reason::NoEvidence;
    } else if (!out.attackScoreOk) {
        out.attackReason = Reason::AttackScoreTooLow;
    } else {
        out.attackReason = Reason::None;
    }

    if (!out.evidenceOk) {
        out.releaseReason = Reason::NoEvidence;
    } else if (!out.releaseScoreOk) {
        out.releaseReason = Reason::ReleaseScoreTooLow;
    } else {
        out.releaseReason = Reason::None;
    }
    out.matched = out.attackOk;

    return out;
}

inline bool passes(const detection::FrequencyBandMeasurementPacket& evidence, const Values& values) {
    return evaluate(evidence, values).matched;
}

inline void buildFailReason(const detection::FrequencyBandMeasurementPacket& evidence,
                            const Values& values,
                            char* out,
                            size_t outSize) {
    if (out == nullptr || outSize == 0) {
        return;
    }

    out[0] = '\0';

    const FrequencyMatchGateResult eval = evaluate(evidence, values);
    switch (eval.attackReason) {
        case Reason::None:
            snprintf(out, outSize, "none");
            break;
        case Reason::NoEvidence:
            snprintf(out, outSize, "freq unavailable");
            break;
        case Reason::AttackScoreTooLow:
            snprintf(out, outSize, "freq score too low");
            break;
        case Reason::AttackContrastTooLow:
            snprintf(out, outSize, "freqContrast too low");
            break;
        case Reason::AttackScoreAndContrastTooLow:
            snprintf(out, outSize, "freqContrast too low,freq score too low");
            break;
        case Reason::ReleaseScoreTooLow:
            snprintf(out, outSize, "freq release score too low");
            break;
        case Reason::ReleaseContrastTooLow:
            snprintf(out, outSize, "freq release contrast too low");
            break;
        case Reason::ReleaseScoreAndContrastTooLow:
            snprintf(out, outSize, "freq release contrast too low,freq release score too low");
            break;
    }
}

} // namespace FrequencyMatchCriteria
