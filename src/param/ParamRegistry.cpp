#include "ParamRegistry.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

namespace param {

namespace {

bool parseBool(const char* valueText, bool& outValue) {
    if (strcasecmp(valueText, "1") == 0 || strcasecmp(valueText, "true") == 0 || strcasecmp(valueText, "on") == 0) {
        outValue = true;
        return true;
    }
    if (strcasecmp(valueText, "0") == 0 || strcasecmp(valueText, "false") == 0 || strcasecmp(valueText, "off") == 0) {
        outValue = false;
        return true;
    }
    return false;
}

} // namespace

bool ParamRegistry::addBinding(const ParamBinding& binding) {
    if (binding.path == nullptr || binding.field == nullptr) {
        return false;
    }
    if (find(binding.path) != nullptr) {
        return false;
    }
    if (_count >= kMaxParams) {
        return false;
    }

    _bindings[_count++] = binding;
    return true;
}

bool ParamRegistry::addUInt16(ParamId id, ModuleId module, const char* path, uint16_t* field, uint16_t minValue, uint16_t maxValue) {
    ParamBinding binding{id, module, ParamType::UInt16, path, field, static_cast<float>(minValue), static_cast<float>(maxValue)};
    return addBinding(binding);
}

bool ParamRegistry::addUInt32(ParamId id, ModuleId module, const char* path, uint32_t* field, uint32_t minValue, uint32_t maxValue) {
    ParamBinding binding{id, module, ParamType::UInt32, path, field, static_cast<float>(minValue), static_cast<float>(maxValue)};
    return addBinding(binding);
}

bool ParamRegistry::addFloat(ParamId id, ModuleId module, const char* path, float* field, float minValue, float maxValue) {
    ParamBinding binding{id, module, ParamType::Float, path, field, minValue, maxValue};
    return addBinding(binding);
}

bool ParamRegistry::addBool(ParamId id, ModuleId module, const char* path, bool* field) {
    ParamBinding binding{id, module, ParamType::Bool, path, field, 0.0f, 1.0f};
    return addBinding(binding);
}

const ParamBinding* ParamRegistry::find(const char* path) const {
    if (path == nullptr) {
        return nullptr;
    }
    for (size_t i = 0; i < _count; ++i) {
        if (strcasecmp(_bindings[i].path, path) == 0) {
            return &_bindings[i];
        }
    }
    return nullptr;
}

ParamSetStatus ParamRegistry::applyValue(const ParamBinding& binding, const char* valueText) const {
    if (valueText == nullptr || *valueText == '\0') {
        return ParamSetStatus::ParseError;
    }

    switch (binding.type) {
        case ParamType::Float: {
            char* end = nullptr;
            const float parsed = strtof(valueText, &end);
            if (end == valueText) {
                return ParamSetStatus::ParseError;
            }
            if (parsed < binding.minValue || parsed > binding.maxValue) {
                return ParamSetStatus::OutOfRange;
            }
            *static_cast<float*>(binding.field) = parsed;
            return ParamSetStatus::Ok;
        }
        case ParamType::UInt32: {
            char* end = nullptr;
            const unsigned long parsed = strtoul(valueText, &end, 10);
            if (end == valueText) {
                return ParamSetStatus::ParseError;
            }
            if (static_cast<float>(parsed) < binding.minValue || static_cast<float>(parsed) > binding.maxValue) {
                return ParamSetStatus::OutOfRange;
            }
            *static_cast<uint32_t*>(binding.field) = static_cast<uint32_t>(parsed);
            return ParamSetStatus::Ok;
        }
        case ParamType::UInt16: {
            char* end = nullptr;
            const unsigned long parsed = strtoul(valueText, &end, 10);
            if (end == valueText) {
                return ParamSetStatus::ParseError;
            }
            if (static_cast<float>(parsed) < binding.minValue || static_cast<float>(parsed) > binding.maxValue) {
                return ParamSetStatus::OutOfRange;
            }
            *static_cast<uint16_t*>(binding.field) = static_cast<uint16_t>(parsed);
            return ParamSetStatus::Ok;
        }
        case ParamType::Bool: {
            bool parsed = false;
            if (!parseBool(valueText, parsed)) {
                return ParamSetStatus::ParseError;
            }
            *static_cast<bool*>(binding.field) = parsed;
            return ParamSetStatus::Ok;
        }
    }

    return ParamSetStatus::ParseError;
}

void ParamRegistry::printValue(Print& out, const ParamBinding& binding) const {
    switch (binding.type) {
        case ParamType::Float:
            out.print(*static_cast<const float*>(binding.field), 3);
            break;
        case ParamType::UInt32:
            out.print(*static_cast<const uint32_t*>(binding.field));
            break;
        case ParamType::UInt16:
            out.print(*static_cast<const uint16_t*>(binding.field));
            break;
        case ParamType::Bool:
            out.print(*static_cast<const bool*>(binding.field) ? 1 : 0);
            break;
    }
}

namespace {
const char* paramTypeName(ParamType type) {
    switch (type) {
        case ParamType::UInt16:
            return "uint16";
        case ParamType::UInt32:
            return "uint32";
        case ParamType::Float:
            return "float";
        case ParamType::Bool:
            return "bool";
    }
    return "unknown";
}
} // namespace

void ParamRegistry::list(Print& out) const {
    for (size_t i = 0; i < _count; ++i) {
        const ParamBinding& binding = _bindings[i];
        out.print("PARAM path=");
        out.print(binding.path);
        out.print(" module=");
        out.print(moduleName(binding.module));
        out.print(" type=");
        out.println(paramTypeName(binding.type));
    }
}

void ParamRegistry::dump(Print& out) const {
    for (size_t i = 0; i < _count; ++i) {
        const ParamBinding& binding = _bindings[i];
        out.print("PARAM path=");
        out.print(binding.path);
        out.print(" module=");
        out.print(moduleName(binding.module));
        out.print(" type=");
        out.print(paramTypeName(binding.type));
        out.print(" value=");
        printValue(out, binding);
        out.println();
    }
}

} // namespace param
