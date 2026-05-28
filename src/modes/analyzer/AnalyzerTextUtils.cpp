#include "AnalyzerTextUtils.h"

#include "AnalyzerApp.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

bool startsWithToken(const char* line, const char* token) {
    return strncmp(line, token, strlen(token)) == 0;
}

bool equalsIgnoreCase(const char* a, const char* b) {
    while (*a != '\0' && *b != '\0') {
        if (toupper(static_cast<unsigned char>(*a)) != toupper(static_cast<unsigned char>(*b))) {
            return false;
        }
        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

bool startsWithTokenIgnoreCase(const char* line, const char* token) {
    while (*token != '\0') {
        if (*line == '\0') {
            return false;
        }
        if (toupper(static_cast<unsigned char>(*line)) != toupper(static_cast<unsigned char>(*token))) {
            return false;
        }
        ++line;
        ++token;
    }

    return true;
}

uint32_t analyzerLogFlagsFromLevel(unsigned long level) {
    if (level == 0) {
        return AnalyzerApp::ANALYZER_LOG_NONE;
    }
    if (level == 1) {
        return AnalyzerApp::DEFAULT_ANALYZER_LOG_FLAGS;
    }
    return AnalyzerApp::ANALYZER_LOG_SUMMARY |
           AnalyzerApp::ANALYZER_LOG_TRIAL |
           AnalyzerApp::ANALYZER_LOG_CANDIDATE |
           AnalyzerApp::ANALYZER_LOG_EXPLAIN;
}

uint32_t analyzerLogFlagsFromToken(const char* token) {
    if (token == nullptr || *token == '\0') {
        return AnalyzerApp::DEFAULT_ANALYZER_LOG_FLAGS;
    }

    if (equalsIgnoreCase(token, "default")) {
        return AnalyzerApp::DEFAULT_ANALYZER_LOG_FLAGS;
    }
    if (equalsIgnoreCase(token, "quiet") || equalsIgnoreCase(token, "none")) {
        return AnalyzerApp::ANALYZER_LOG_NONE;
    }
    if (equalsIgnoreCase(token, "full")) {
        return AnalyzerApp::ANALYZER_LOG_SUMMARY |
               AnalyzerApp::ANALYZER_LOG_TRIAL |
               AnalyzerApp::ANALYZER_LOG_CANDIDATE |
               AnalyzerApp::ANALYZER_LOG_EXPLAIN;
    }

    char buffer[64];
    strncpy(buffer, token, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    uint32_t flags = AnalyzerApp::ANALYZER_LOG_NONE;
    char* savePtr = nullptr;
    char* part = strtok_r(buffer, ",+|", &savePtr);
    while (part != nullptr) {
        if (equalsIgnoreCase(part, "summary")) {
            flags |= AnalyzerApp::ANALYZER_LOG_SUMMARY;
        } else if (equalsIgnoreCase(part, "trial")) {
            flags |= AnalyzerApp::ANALYZER_LOG_TRIAL;
        } else if (equalsIgnoreCase(part, "candidate")) {
            flags |= AnalyzerApp::ANALYZER_LOG_CANDIDATE;
        } else if (equalsIgnoreCase(part, "diag")) {
            flags |= AnalyzerApp::ANALYZER_LOG_DIAG;
        } else if (equalsIgnoreCase(part, "explain")) {
            flags |= AnalyzerApp::ANALYZER_LOG_EXPLAIN;
        } else if (equalsIgnoreCase(part, "custom")) {
            flags |= AnalyzerApp::ANALYZER_LOG_CUSTOM;
        } else if (equalsIgnoreCase(part, "default")) {
            flags |= AnalyzerApp::DEFAULT_ANALYZER_LOG_FLAGS;
        } else if (equalsIgnoreCase(part, "full")) {
            flags |= AnalyzerApp::ANALYZER_LOG_SUMMARY |
                     AnalyzerApp::ANALYZER_LOG_TRIAL |
                     AnalyzerApp::ANALYZER_LOG_CANDIDATE |
                     AnalyzerApp::ANALYZER_LOG_EXPLAIN;
        } else if (equalsIgnoreCase(part, "quiet") || equalsIgnoreCase(part, "none")) {
            flags = AnalyzerApp::ANALYZER_LOG_NONE;
        }
        part = strtok_r(nullptr, ",+|", &savePtr);
    }

    return flags;
}
