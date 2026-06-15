#pragma once

#include "DetectorReport.h"

// Generic detector report rendering entry points used by Analyzer/SEQ output.
// Detector-specific detail lives behind these helpers.
namespace detection {

void printDetectorReportGenericLine(const char* prefix, unsigned long trial, const DetectorReport* report);
void printDetectorDetailLine(const char* prefix, const DetectorReport* report);

} // namespace detection
