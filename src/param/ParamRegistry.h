#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ParamTypes.h"

class Print;

/*
ParamRegistry

Flat, fixed-capacity table of bound fields. A module registers a pointer to
one of its own tuning fields under a public dotted path; the registry owns
lookup, parsing, range validation, and writing the field. The owning module
still decides what the value means and how to apply it after a change.

This is intentionally not a generic config framework: no persistence, no
dynamic allocation, no nested schemas. Serial PARAM commands are the only
transport today. A later transport (VEKTOR) should route through the same
find()/applyValue() path rather than touching module fields directly, so the
path/module vocabulary here doubles as the future external resource identity.
*/
namespace param {

struct ParamBinding {
    ParamId id;
    ModuleId module;
    ParamType type;
    const char* path;
    void* field;
    float minValue;
    float maxValue;
};

class ParamRegistry {
public:
    static constexpr size_t kMaxParams = 32;

    bool addUInt16(ParamId id, ModuleId module, const char* path, uint16_t* field, uint16_t minValue, uint16_t maxValue);
    bool addUInt32(ParamId id, ModuleId module, const char* path, uint32_t* field, uint32_t minValue, uint32_t maxValue);
    bool addFloat(ParamId id, ModuleId module, const char* path, float* field, float minValue, float maxValue);
    bool addBool(ParamId id, ModuleId module, const char* path, bool* field);

    size_t count() const { return _count; }
    const ParamBinding* find(const char* path) const;

    // Parses valueText per the binding's type and writes the bound field on
    // success. Returns why it was rejected otherwise. Never touches fields
    // outside this binding.
    ParamSetStatus applyValue(const ParamBinding& binding, const char* valueText) const;

    void printValue(Print& out, const ParamBinding& binding) const;

    // PARAM LIST: one line per registered param, no values.
    void list(Print& out) const;
    // PARAM DUMP: one line per registered param, with current values.
    void dump(Print& out) const;

private:
    bool addBinding(const ParamBinding& binding);

    ParamBinding _bindings[kMaxParams] = {};
    size_t _count = 0;
};

} // namespace param
