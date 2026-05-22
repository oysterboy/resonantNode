#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../inspector/InspectorTypes.h"

/*
FrequencyMatchEvaluation

Threshold parsing and evaluation helpers for frequency-match evidence.
This is signal/profile tuning support, not PatternRules.
*/
namespace FrequencyMatchEvaluation {

struct Values {
    float scoreMin = 50000.0f;
    float contrastMin = 20.0f;
};

struct ClassifierTuning {
    Values frequency = {};
};

enum class Reason {
    None,
    NoEvidence,
    InvalidWindow,
    ScoreTooLow,
    ContrastTooLow,
    ScoreAndContrastTooLow,
};

struct Evaluation {
    bool present = false;
    bool validWindow = false;
    bool scoreOk = false;
    bool contrastOk = false;
    bool matched = false;
    float score = 0.0f;
    float contrast = 0.0f;
    float scoreMin = 0.0f;
    float contrastMin = 0.0f;
    Reason reason = Reason::None;
};

inline bool parseToken(const char* token, Values& values) {
    if (token == nullptr) {
        return false;
    }
    if (strncmp(token, "freqScore=", 10) == 0) {
        values.scoreMin = strtof(token + 10, nullptr);
        return true;
    }
    if (strncmp(token, "freqContrast=", 13) == 0) {
        values.contrastMin = strtof(token + 13, nullptr);
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
        case Reason::ScoreTooLow:
            return "freq_score_too_low";
        case Reason::ContrastTooLow:
            return "freq_contrast_too_low";
        case Reason::ScoreAndContrastTooLow:
            return "freq_score_and_contrast_too_low";
    }

    return "unknown";
}

inline Evaluation evaluate(const detection::FrequencyEvidence& evidence, const Values& values) {
    Evaluation out;
    out.present = evidence.present;
    out.validWindow = evidence.validWindow;
    out.score = evidence.score;
    out.contrast = evidence.spectralContrast;
    out.scoreMin = values.scoreMin;
    out.contrastMin = values.contrastMin;

    out.scoreOk = evidence.score >= values.scoreMin;
    out.contrastOk = evidence.spectralContrast >= values.contrastMin;
    out.matched = evidence.present && evidence.validWindow && out.scoreOk && out.contrastOk;

    if (!evidence.present) {
        out.reason = Reason::NoEvidence;
    } else if (!evidence.validWindow) {
        out.reason = Reason::InvalidWindow;
    } else if (!out.scoreOk && !out.contrastOk) {
        out.reason = Reason::ScoreAndContrastTooLow;
    } else if (!out.scoreOk) {
        out.reason = Reason::ScoreTooLow;
    } else if (!out.contrastOk) {
        out.reason = Reason::ContrastTooLow;
    } else {
        out.reason = Reason::None;
    }

    return out;
}

inline bool passes(const detection::FrequencyEvidence& evidence, const Values& values) {
    return evaluate(evidence, values).matched;
}

inline void buildFailReason(const detection::FrequencyEvidence& evidence,
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
        case Reason::ScoreTooLow:
            snprintf(out, outSize, "freq score too low");
            break;
        case Reason::ContrastTooLow:
            snprintf(out, outSize, "freqContrast too low");
            break;
        case Reason::ScoreAndContrastTooLow:
            snprintf(out, outSize, "freqContrast too low,freq score too low");
            break;
    }
}

} // namespace FrequencyMatchEvaluation
