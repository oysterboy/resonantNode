#pragma once

#include "../DetectionTypes.h"
#include "DetectorReject.h"

// Canonical detector and occurrence vocabulary for report labels and SEQ text.
// Keep detector-facing names here so Analyzer does not own the wording.
namespace detection {

const char* detectorIdName(DetectorId detectorId);
const char* detectorRejectClassName(DetectorRejectClass rejectClass);
const char* occurrenceSourceName(DetectorId detectorId);

} // namespace detection
