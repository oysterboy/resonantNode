#pragma once

#include "../../app/TextParse.h"

/*
The Analyzer console's text helpers, now forwarding to app/TextParse.h, where
the three prefix kinds are defined once (review B7, step 11f). Plain prefix
matches, as the Analyzer always used them.
*/
inline bool startsWithToken(const char* line, const char* token) {
    return text_parse::startsWith(line, token);
}

inline bool equalsIgnoreCase(const char* a, const char* b) {
    return text_parse::equalsIgnoreCase(a, b);
}

inline bool startsWithTokenIgnoreCase(const char* line, const char* token) {
    return text_parse::startsWithIgnoreCase(line, token);
}
