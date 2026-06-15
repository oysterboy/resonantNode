#pragma once

#include "../DetectorReport.h"

// Frequency-match detector-specific report/detail rendering helpers.
// Keep frequency wording and field printing close to the frequency detector code.
namespace detection::frequency {

void printFrequencyMatchDetailLine(const char* prefix, const detection::DetectorReport* report);

} // namespace detection::frequency
