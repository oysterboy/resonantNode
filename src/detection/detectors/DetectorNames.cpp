#include "DetectorNames.h"

// Implementation for the canonical detector/occurrence naming helpers.
namespace detection {

const char* detectorIdName(DetectorId detectorId) {
    switch (detectorId) {
        case DetectorId::ScalarTransient:
            return "scalar_transient";
        case DetectorId::FrequencyMatch:
            return "frequency_match";
        case DetectorId::Unknown:
        default:
            return "unknown";
    }
}

const char* detectorRejectClassName(DetectorRejectClass rejectClass) {
    switch (rejectClass) {
        case DetectorRejectClass::None:
            return "none";
        case DetectorRejectClass::Threshold:
            return "threshold";
        case DetectorRejectClass::Timing:
            return "timing";
        case DetectorRejectClass::Strength:
            return "strength";
        case DetectorRejectClass::Cooldown:
            return "cooldown";
        case DetectorRejectClass::State:
            return "state";
        case DetectorRejectClass::Window:
            return "window";
        case DetectorRejectClass::Unknown:
        default:
            return "unknown";
    }
}

const char* occurrenceSourceName(DetectorId detectorId) {
    switch (detectorId) {
        case DetectorId::ScalarTransient:
            return "scalar";
        case DetectorId::FrequencyMatch:
            return "frequency";
        case DetectorId::Unknown:
        default:
            return "none";
    }
}

} // namespace detection
