#pragma once

#include "VerdictTypes.h"

namespace detection {

inline const char* verdictTypeName(VerdictType type) {
    switch (type) {
        case VerdictType::None:
            return "none";
        case VerdictType::SinglePulse:
            return "single_pulse";
    }

    return "unknown";
}

inline const char* verdictReasonName(VerdictReasonCode code) {
    switch (code) {
        case VerdictReasonCode::None:
            return "none";
        case VerdictReasonCode::FromOccurrence:
            return "from_occurrence";
        case VerdictReasonCode::SupportRequirementFailed:
            return "support_requirement_failed";
    }

    return "unknown";
}

inline const char* verdictRejectReasonName(VerdictRejectReason reason) {
    switch (reason) {
        case VerdictRejectReason::None:
            return "none";
        case VerdictRejectReason::MissingSupport:
            return "missing_support";
        case VerdictRejectReason::SupportTooLow:
            return "support_too_low";
    }

    return "unknown";
}

} // namespace detection
