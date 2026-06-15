#pragma once

#include "../DetectorReport.h"

// Scalar-transient detector-specific report/detail rendering helpers.
// Keep scalar wording and field printing close to the scalar detector code.
namespace detection::scalar {

void printScalarTransientDetailLine(const char* prefix, const detection::DetectorReport* report);

} // namespace detection::scalar
