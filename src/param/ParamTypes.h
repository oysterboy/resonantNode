#pragma once

#include <stdint.h>

/*
Param core vocabulary.

Minimal runtime-only bound-field param system. See
docs/roadmaps/roadmap-param-config.md (PAR-002) for the intended shape:
flat ParamBindings, module-owned meaning, no persistence yet.

Paths are the stable public identity. ParamId is internal identity, kept
separate so a future transport (Serial today, VEKTOR later) never needs to
know about field pointers or storage details.
*/
namespace param {

enum class ModuleId : uint8_t {
    Node,
    Detection,
    Behavior,
    Output,
};

inline const char* moduleName(ModuleId module) {
    switch (module) {
        case ModuleId::Node:
            return "node";
        case ModuleId::Detection:
            return "detection";
        case ModuleId::Behavior:
            return "behavior";
        case ModuleId::Output:
            return "output";
    }
    return "unknown";
}

enum class ParamType : uint8_t {
    UInt16,
    UInt32,
    Float,
    Bool,
};

// Stable internal identity for a registered param. Grows as modules register
// more fields; not reused across removed params.
enum class ParamId : uint16_t {
    Detection_FreqAttackScoreMin,
    Detection_FreqReleaseScoreMin,
    Detection_FreqAttackContrastMin,
    Detection_FreqReleaseContrastMin,
};

enum class ParamSetStatus : uint8_t {
    Ok,
    UnknownParam,
    ParseError,
    OutOfRange,
};

inline const char* paramSetStatusName(ParamSetStatus status) {
    switch (status) {
        case ParamSetStatus::Ok:
            return "ok";
        case ParamSetStatus::UnknownParam:
            return "unknown_param";
        case ParamSetStatus::ParseError:
            return "parse_error";
        case ParamSetStatus::OutOfRange:
            return "out_of_range";
    }
    return "unknown";
}

} // namespace param
