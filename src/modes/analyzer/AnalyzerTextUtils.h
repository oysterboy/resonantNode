#pragma once

#include <stdint.h>

bool startsWithToken(const char* line, const char* token);
bool equalsIgnoreCase(const char* a, const char* b);
bool startsWithTokenIgnoreCase(const char* line, const char* token);
uint32_t analyzerLogFlagsFromLevel(unsigned long level);
uint32_t analyzerLogFlagsFromToken(const char* token);
