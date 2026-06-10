#pragma once

#include "DetectionTypes.h"

namespace detection {

/*
DetectorDescriptor

Minimal canonical detector identity descriptor.
Runtime code may still rely on legacy source naming during migration.
*/
struct DetectorDescriptor {
    DetectorId detectorId = DetectorId::Unknown;
    OccurrenceType occurrenceType = OccurrenceType::None;
    const char* name = "unknown";
};

} // namespace detection
