#pragma once

#include <math.h>
#include <stdlib.h>

// Number parsing for console commands, shared by the Analyzer and the Node.
namespace text_parse {

// A finite number with nothing after it. On anything else the value is left
// unchanged: strtof(..., nullptr) turned "x" into 0 (the node then triggered
// on noise) and accepted "nan" (detection off) (issue #31 R2-4). Moved here
// from FrequencyMatchCriteria when that detector was removed (step 7d).
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

}  // namespace text_parse
