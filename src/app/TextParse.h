#pragma once

#include <ctype.h>
#include <errno.h>
#include <math.h>

#include <limits>
#include <stdlib.h>
#include <string.h>

// Text helpers for console commands, shared by the Node, the Analyzer and the
// Emitter (step 11e item 8): numbers are parsed whole or refused, a key's value
// is found at its '=', and a command's rest is found after its verb, so no
// parser counts key lengths by hand (that got four wrong once).
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

// A plain decimal with nothing after it; on anything else the value is left
// unchanged (issue #31 R2-4: "x" used to become 0). The result is narrowed to
// T as the caller's field needs it.
template <typename T>
inline bool parseUnsigned(const char* text, T& out) {
    if (text == nullptr || *text < '0' || *text > '9') {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const unsigned long parsed = strtoul(text, &end, 10);
    // Overlong numbers saturated to ULONG_MAX, and a value beyond T wrapped in the cast (deep review L9).
    if (*end != '\0' || errno == ERANGE || parsed > static_cast<unsigned long>(std::numeric_limits<T>::max())) {
        return false;
    }
    out = static_cast<T>(parsed);
    return true;
}

// The text after the first '=' of a key=value token; the empty string when
// the token has no '='.
inline const char* tokenValue(const char* token) {
    const char* equals = strchr(token, '=');
    return equals != nullptr ? equals + 1 : token + strlen(token);
}

// The text after `verb` (which the caller has already matched at the start
// of `line`, case-insensitively or not), with the spaces skipped.
inline const char* rest(const char* line, const char* verb) {
    const char* p = line + strlen(verb);
    while (*p == ' ') {
        ++p;
    }
    return p;
}

// Prefix matches. Three kinds exist on purpose, with different meanings:
// - startsWith: plain prefix, case-sensitive (the Emitter's link commands).
// - startsWithIgnoreCase: plain prefix, any case (the Analyzer's console).
// - startsWithWordIgnoreCase: the prefix ends on a token boundary, the end or a space, unless it ends
//   in '=' (key=value); "RB rebasefoo" is not "RB rebase" (the Node's console, issue #31 R2-19).
inline bool startsWith(const char* line, const char* prefix) {
    return strncmp(line, prefix, strlen(prefix)) == 0;
}

inline bool startsWithIgnoreCase(const char* line, const char* prefix) {
    while (*prefix != '\0') {
        if (*line == '\0' ||
            toupper(static_cast<unsigned char>(*line)) != toupper(static_cast<unsigned char>(*prefix))) {
            return false;
        }
        ++line;
        ++prefix;
    }
    return true;
}

inline bool startsWithWordIgnoreCase(const char* line, const char* prefix) {
    char last = '\0';
    while (*prefix != '\0') {
        if (*line == '\0' ||
            toupper(static_cast<unsigned char>(*line)) != toupper(static_cast<unsigned char>(*prefix))) {
            return false;
        }
        last = *prefix;
        ++line;
        ++prefix;
    }
    return last == '=' || *line == '\0' || *line == ' ';
}

// Whole-string match, case-insensitive (command verbs).
inline bool equalsIgnoreCase(const char* a, const char* b) {
    while (*a != '\0' && *b != '\0') {
        if (toupper(static_cast<unsigned char>(*a)) != toupper(static_cast<unsigned char>(*b))) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

}  // namespace text_parse
