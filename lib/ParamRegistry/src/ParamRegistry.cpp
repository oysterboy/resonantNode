#include "ParamRegistry.h"

#include <Arduino.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

namespace param {

const char* paramSetStatusName(ParamSetStatus status) {
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

// The whole value must parse: "1500abc" is an error, not 1500 (issue #31).
bool parsedToEnd(const char* valueText, const char* end) {
    if (end == valueText) {
        return false;
    }
    while (*end == ' ' || *end == '\t') {
        ++end;
    }
    return *end == '\0';
}

// An unsigned value: digits only after optional leading blanks. strtoul
// accepts "-1" and wraps it to ULONG_MAX, so a sign is rejected up front.
ParamSetStatus parseUnsigned(const char* valueText, uint32_t& out) {
    const char* p = valueText;
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    if (*p < '0' || *p > '9') {
        return ParamSetStatus::ParseError;
    }
    char* end = nullptr;
    errno = 0;
    const unsigned long parsed = strtoul(p, &end, 10);
    if (!parsedToEnd(p, end)) {
        return ParamSetStatus::ParseError;
    }
    if (errno == ERANGE || parsed > 0xFFFFFFFFUL) {
        return ParamSetStatus::OutOfRange;
    }
    out = static_cast<uint32_t>(parsed);
    return ParamSetStatus::Ok;
}

}  // namespace

bool ParamRegistry::addBinding(const ParamBinding& binding) {
    if (binding.path == nullptr || binding.field == nullptr) {
        return false;
    }
    if (find(binding.path) != nullptr) {
        return false;
    }
    for (size_t i = 0; i < _count; ++i) {
        if (_bindings[i].id == binding.id) {
            return false;
        }
    }
    if (_count >= kMaxParams) {
        return false;
    }

    _bindings[_count++] = binding;
    return true;
}

bool ParamRegistry::addUInt16Raw(uint16_t id, uint8_t group, const char* path, uint16_t* field, uint16_t minValue, uint16_t maxValue) {
    ParamBinding binding{id, group, ParamType::UInt16, path, field, static_cast<float>(minValue), static_cast<float>(maxValue),
                         minValue, maxValue};
    return addBinding(binding);
}

bool ParamRegistry::addUInt32Raw(uint16_t id, uint8_t group, const char* path, uint32_t* field, uint32_t minValue, uint32_t maxValue) {
    ParamBinding binding{id, group, ParamType::UInt32, path, field, static_cast<float>(minValue), static_cast<float>(maxValue),
                         minValue, maxValue};
    return addBinding(binding);
}

bool ParamRegistry::addFloatRaw(uint16_t id, uint8_t group, const char* path, float* field, float minValue, float maxValue) {
    ParamBinding binding{id, group, ParamType::Float, path, field, minValue, maxValue, 0, 0};
    return addBinding(binding);
}

bool ParamRegistry::addBoolRaw(uint16_t id, uint8_t group, const char* path, bool* field) {
    ParamBinding binding{id, group, ParamType::Bool, path, field, 0.0f, 1.0f, 0, 1};
    return addBinding(binding);
}

const char* ParamRegistry::groupName(uint8_t group) const {
    const char* name = _groupNamer != nullptr ? _groupNamer(group) : nullptr;
    return name != nullptr ? name : "unknown";
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
            if (!parsedToEnd(valueText, end)) {
                return ParamSetStatus::ParseError;
            }
            // NaN fails both comparisons below and every threshold compare
            // after it; inf is caught by the range check.
            if (isnan(parsed)) {
                return ParamSetStatus::ParseError;
            }
            if (parsed < binding.minValue || parsed > binding.maxValue) {
                return ParamSetStatus::OutOfRange;
            }
            *static_cast<float*>(binding.field) = parsed;
            return ParamSetStatus::Ok;
        }
        case ParamType::UInt32:
        case ParamType::UInt16: {
            uint32_t parsed = 0;
            const ParamSetStatus status = parseUnsigned(valueText, parsed);
            if (status != ParamSetStatus::Ok) {
                return status;
            }
            if (parsed < binding.minUnsigned || parsed > binding.maxUnsigned) {
                return ParamSetStatus::OutOfRange;
            }
            if (binding.type == ParamType::UInt32) {
                *static_cast<uint32_t*>(binding.field) = parsed;
            } else {
                *static_cast<uint16_t*>(binding.field) = static_cast<uint16_t>(parsed);
            }
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

double ParamRegistry::numericValue(const ParamBinding& binding) const {
    switch (binding.type) {
        case ParamType::Float:
            return static_cast<double>(*static_cast<const float*>(binding.field));
        case ParamType::UInt32:
            return static_cast<double>(*static_cast<const uint32_t*>(binding.field));
        case ParamType::UInt16:
            return static_cast<double>(*static_cast<const uint16_t*>(binding.field));
        case ParamType::Bool:
            return *static_cast<const bool*>(binding.field) ? 1.0 : 0.0;
    }
    return 0.0;
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

void ParamRegistry::list(Print& out) const {
    for (size_t i = 0; i < _count; ++i) {
        const ParamBinding& binding = _bindings[i];
        out.print("PARAM path=");
        out.print(binding.path);
        out.print(" module=");
        out.print(groupName(binding.group));
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
        out.print(groupName(binding.group));
        out.print(" type=");
        out.print(paramTypeName(binding.type));
        out.print(" value=");
        printValue(out, binding);
        out.println();
    }
}

uint32_t ParamRegistry::valueBits(const ParamBinding& binding) const {
    switch (binding.type) {
        case ParamType::Float: {
            uint32_t bits = 0;
            memcpy(&bits, binding.field, sizeof(bits));
            return bits;
        }
        case ParamType::UInt32:
            return *static_cast<const uint32_t*>(binding.field);
        case ParamType::UInt16:
            return *static_cast<const uint16_t*>(binding.field);
        case ParamType::Bool:
            return *static_cast<const bool*>(binding.field) ? 1U : 0U;
    }
    return 0;
}

bool ParamRegistry::bitsInRange(const ParamBinding& binding, uint32_t bits) const {
    switch (binding.type) {
        case ParamType::Float: {
            float value = 0.0f;
            memcpy(&value, &bits, sizeof(value));
            return !isnan(value) && value >= binding.minValue && value <= binding.maxValue;
        }
        case ParamType::UInt32:
        case ParamType::UInt16:
            return bits >= binding.minUnsigned && bits <= binding.maxUnsigned;
        case ParamType::Bool:
            return bits <= 1U;
    }
    return false;
}

void ParamRegistry::writeBits(const ParamBinding& binding, uint32_t bits) const {
    switch (binding.type) {
        case ParamType::Float:
            memcpy(binding.field, &bits, sizeof(bits));
            break;
        case ParamType::UInt32:
            *static_cast<uint32_t*>(binding.field) = bits;
            break;
        case ParamType::UInt16:
            *static_cast<uint16_t*>(binding.field) = static_cast<uint16_t>(bits);
            break;
        case ParamType::Bool:
            *static_cast<bool*>(binding.field) = bits != 0U;
            break;
    }
}

void ParamRegistry::printBits(Print& out, const ParamBinding& binding, uint32_t bits) const {
    if (binding.type == ParamType::Float) {
        float value = 0.0f;
        memcpy(&value, &bits, sizeof(value));
        out.print(value, 3);
    } else {
        out.print(bits);
    }
}

void ParamRegistry::captureDefaults() {
    for (size_t i = 0; i < _count; ++i) {
        _bindings[i].defaultBits = valueBits(_bindings[i]);
    }
}

void ParamRegistry::restoreDefaults() const {
    for (size_t i = 0; i < _count; ++i) {
        writeBits(_bindings[i], _bindings[i].defaultBits);
    }
}

ParamSaveReport ParamRegistry::save(ParamStore& store) const {
    ParamSaveReport report;
    for (size_t i = 0; i < _count; ++i) {
        const ParamBinding& binding = _bindings[i];
        const StoredParam entry{StoredParam::kVersion, static_cast<uint8_t>(binding.type), binding.id,
                                valueBits(binding), binding.defaultBits};
        StoredParam existing{};
        if (store.read(binding.id, existing) && existing.version == entry.version && existing.type == entry.type &&
            existing.id == entry.id && existing.valueBits == entry.valueBits &&
            existing.savedDefaultBits == entry.savedDefaultBits) {
            ++report.unchanged;
            continue;
        }
        if (store.write(entry)) {
            ++report.written;
        } else {
            ++report.failed;
        }
    }
    return report;
}

ParamLoadReport ParamRegistry::load(ParamStore& store, Print& out) const {
    ParamLoadReport report;
    for (size_t i = 0; i < _count; ++i) {
        const ParamBinding& binding = _bindings[i];
        StoredParam entry{};
        if (!store.read(binding.id, entry)) {
            continue;
        }
        ++report.stored;
        if (entry.version != StoredParam::kVersion || entry.id != binding.id ||
            entry.type != static_cast<uint8_t>(binding.type) || !bitsInRange(binding, entry.valueBits)) {
            ++report.invalid;
            out.print("PARAM load path=");
            out.print(binding.path);
            out.println(" status=invalid kept=current");
            continue;
        }
        if (entry.savedDefaultBits != binding.defaultBits) {
            ++report.stale;
            out.print("PARAM stale path=");
            out.print(binding.path);
            out.print(" stored=");
            printBits(out, binding, entry.valueBits);
            out.print(" saved_default=");
            printBits(out, binding, entry.savedDefaultBits);
            out.print(" firmware_default=");
            printBits(out, binding, binding.defaultBits);
            out.println(" using=stored");
        }
        writeBits(binding, entry.valueBits);
        ++report.applied;
        if (entry.valueBits != binding.defaultBits) {
            out.print("PARAM load path=");
            out.print(binding.path);
            out.print(" value=");
            printBits(out, binding, entry.valueBits);
            out.print(" default=");
            printBits(out, binding, binding.defaultBits);
            out.println();
        }
    }
    return report;
}

}  // namespace param
