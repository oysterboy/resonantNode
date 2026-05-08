#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "DetectionPipeline.h"

namespace FrequencyLoggingTuning {

struct Values {
    float scoreMin = 50000.0f;
    float contrastMin = 20.0f;
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

inline bool passes(const DetectionPipeline::FrequencyEvidence& evidence, const Values& values) {
    return evidence.present &&
           evidence.score >= values.scoreMin &&
           evidence.spectralContrast >= values.contrastMin;
}

inline void buildFailReason(const DetectionPipeline::FrequencyEvidence& evidence,
                            const Values& values,
                            char* out,
                            size_t outSize) {
    if (out == nullptr || outSize == 0) {
        return;
    }

    out[0] = '\0';

    if (!evidence.present) {
        snprintf(out, outSize, "freq unavailable");
        return;
    }

    bool any = false;
    auto appendReason = [&](const char* reason) {
        const size_t used = strlen(out);
        if (used >= outSize - 1) {
            return;
        }
        snprintf(out + used, outSize - used, "%s%s", any ? "," : "", reason);
        any = true;
    };

    if (evidence.spectralContrast < values.contrastMin) {
        appendReason("freqContrast too low");
    }
    if (evidence.score < values.scoreMin) {
        appendReason("freq score too low");
    }

    if (!any) {
        snprintf(out, outSize, "none");
    }
}

} // namespace FrequencyLoggingTuning
