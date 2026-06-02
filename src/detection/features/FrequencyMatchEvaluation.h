#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../inspector/InspectorTypes.h"

/*
FrequencyMatchEvaluation

Threshold parsing and evaluation helpers for frequency-match evidence.
This is occurrence/profile tuning support, not PatternRules.
*/
namespace FrequencyMatchEvaluation {

struct Values {
    float attackScoreMin = 10000.0f;
    float releaseScoreMin = 8000.0f;
    float attackContrastMin = 50.0f;
    float releaseContrastMin = 50.0f;

    // Compatibility mirrors while call sites migrate.
    float scoreMin = attackScoreMin;
    float contrastMin = attackContrastMin;
};

struct ClassifierTuning {
    Values frequency = {};
};

enum class Reason {
    None,
    NoEvidence,
    InvalidWindow,
    AttackScoreTooLow,
    AttackContrastTooLow,
    AttackScoreAndContrastTooLow,
    ReleaseScoreTooLow,
    ReleaseContrastTooLow,
    ReleaseScoreAndContrastTooLow,
};

struct Evaluation {
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

    // Compatibility fields.
    bool present = false;
    bool validWindow = false;
    bool scoreOk = false;
    bool contrastOk = false;
    float scoreMin = 0.0f;
    float contrastMin = 0.0f;
    Reason reason = Reason::None;
};

inline bool parseToken(const char* token, Values& values) {
    if (token == nullptr) {
        return false;
    }
    if (strncmp(token, "freqAttackScore=", 16) == 0) {
        values.attackScoreMin = strtof(token + 16, nullptr);
        values.scoreMin = values.attackScoreMin;
        return true;
    }
    if (strncmp(token, "freqReleaseScore=", 17) == 0) {
        values.releaseScoreMin = strtof(token + 17, nullptr);
        return true;
    }
    if (strncmp(token, "freqAttackContrast=", 19) == 0) {
        values.attackContrastMin = strtof(token + 19, nullptr);
        values.contrastMin = values.attackContrastMin;
        return true;
    }
    if (strncmp(token, "freqReleaseContrast=", 20) == 0) {
        values.releaseContrastMin = strtof(token + 20, nullptr);
        return true;
    }
    if (strncmp(token, "freqScore=", 10) == 0) {
        values.attackScoreMin = strtof(token + 10, nullptr);
        values.scoreMin = values.attackScoreMin;
        return true;
    }
    if (strncmp(token, "freqContrast=", 13) == 0) {
        values.attackContrastMin = strtof(token + 13, nullptr);
        values.contrastMin = values.attackContrastMin;
        return true;
    }
    return false;
}

inline const char* reasonName(Reason reason) {
    switch (reason) {
        case Reason::None:
            return "none";
        case Reason::NoEvidence:
            return "no_frequency_evidence";
        case Reason::InvalidWindow:
            return "frequency_window_invalid";
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

inline Evaluation evaluate(const detection::FrequencyFeatureFrame& evidence, const Values& values) {
    Evaluation out;
    out.evidenceOk = evidence.evidencePresent;
    out.score = evidence.score;
    out.contrast = evidence.spectralContrast;
    out.attackScoreMin = values.attackScoreMin;
    out.releaseScoreMin = values.releaseScoreMin;
    out.attackContrastMin = values.attackContrastMin;
    out.releaseContrastMin = values.releaseContrastMin;

    out.attackScoreOk = evidence.score >= values.attackScoreMin;
    out.attackContrastOk = evidence.spectralContrast >= values.attackContrastMin;
    out.attackOk = out.evidenceOk && out.attackScoreOk && out.attackContrastOk;

    out.releaseScoreOk = evidence.score >= values.releaseScoreMin;
    out.releaseContrastOk = evidence.spectralContrast >= values.releaseContrastMin;
    out.releaseOk = out.evidenceOk && out.releaseScoreOk && out.releaseContrastOk;

    out.matched = out.attackOk;

    if (!out.evidenceOk) {
        out.attackReason = Reason::NoEvidence;
        out.releaseReason = Reason::NoEvidence;
    } else if (!out.attackScoreOk && !out.attackContrastOk) {
        out.attackReason = Reason::AttackScoreAndContrastTooLow;
    } else if (!out.attackScoreOk) {
        out.attackReason = Reason::AttackScoreTooLow;
    } else if (!out.attackContrastOk) {
        out.attackReason = Reason::AttackContrastTooLow;
    } else {
        out.attackReason = Reason::None;
    }

    if (!out.evidenceOk) {
        out.releaseReason = Reason::NoEvidence;
    } else if (!out.releaseScoreOk && !out.releaseContrastOk) {
        out.releaseReason = Reason::ReleaseScoreAndContrastTooLow;
    } else if (!out.releaseScoreOk) {
        out.releaseReason = Reason::ReleaseScoreTooLow;
    } else if (!out.releaseContrastOk) {
        out.releaseReason = Reason::ReleaseContrastTooLow;
    } else {
        out.releaseReason = Reason::None;
    }

    // Compatibility mirrors.
    out.present = out.evidenceOk;
    out.validWindow = out.evidenceOk;
    out.scoreOk = out.attackScoreOk;
    out.contrastOk = out.attackContrastOk;
    out.scoreMin = out.attackScoreMin;
    out.contrastMin = out.attackContrastMin;
    out.reason = out.attackReason;

    return out;
}

inline bool passes(const detection::FrequencyFeatureFrame& evidence, const Values& values) {
    return evaluate(evidence, values).matched;
}

inline void buildFailReason(const detection::FrequencyFeatureFrame& evidence,
                            const Values& values,
                            char* out,
                            size_t outSize) {
    if (out == nullptr || outSize == 0) {
        return;
    }

    out[0] = '\0';

    const Evaluation eval = evaluate(evidence, values);
    switch (eval.reason) {
        case Reason::None:
            snprintf(out, outSize, "none");
            break;
        case Reason::NoEvidence:
            snprintf(out, outSize, "freq unavailable");
            break;
        case Reason::InvalidWindow:
            snprintf(out, outSize, "freq window invalid");
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

} // namespace FrequencyMatchEvaluation
