#pragma once

#include <stddef.h>
#include <stdint.h>

class Print;

/*
ParamRegistry (lib/ParamRegistry, PAR-016)

Flat, fixed-capacity table of bound fields. A module registers a pointer to
one of its own tuning fields under a public dotted path; the registry owns
lookup, parsing, range validation, and writing the field. The owning module
still decides what the value means and how to apply it after a change.

This is intentionally not a generic config framework: no persistence, no
dynamic allocation, no nested schemas. Serial PARAM commands are the only
transport today. A later transport (VEKTOR) should route through the same
find()/applyValue() path rather than touching module fields directly, so the
path vocabulary doubles as the future external resource identity.

The library knows no app vocabulary. A binding carries an opaque numeric id
and group tag; the app defines both (its ParamId and ModuleId enums) and
names the groups through setGroupNamer(). The templated add*() overloads take
the app's enums directly. Nothing here includes the app's headers.
*/
namespace param {

enum class ParamType : uint8_t {
    UInt16,
    UInt32,
    Float,
    Bool,
};

enum class ParamSetStatus : uint8_t {
    Ok,
    UnknownParam,
    ParseError,
    OutOfRange,
};

const char* paramSetStatusName(ParamSetStatus status);
const char* paramTypeName(ParamType type);

struct ParamBinding {
    uint16_t id;
    uint8_t group;
    ParamType type;
    const char* path;
    void* field;
    // Float bounds; the unsigned types use the integer bounds below, so a
    // uint32 range is checked without a float round-trip.
    float minValue;
    float maxValue;
    uint32_t minUnsigned;
    uint32_t maxUnsigned;
};

class ParamRegistry {
public:
    static constexpr size_t kMaxParams = 48;  // 35 used by the Node (22 + 13 behavior.*) since step 10
    using GroupNamer = const char* (*)(uint8_t group);

    // Raw registration. Rejects a null path or field, a duplicate path, a
    // duplicate id, and a full table.
    bool addUInt16Raw(uint16_t id, uint8_t group, const char* path, uint16_t* field, uint16_t minValue, uint16_t maxValue);
    bool addUInt32Raw(uint16_t id, uint8_t group, const char* path, uint32_t* field, uint32_t minValue, uint32_t maxValue);
    bool addFloatRaw(uint16_t id, uint8_t group, const char* path, float* field, float minValue, float maxValue);
    bool addBoolRaw(uint16_t id, uint8_t group, const char* path, bool* field);

    // The app's enums (any type convertible by static_cast to the tag widths).
    template <typename Id, typename Group>
    bool addUInt16(Id id, Group group, const char* path, uint16_t* field, uint16_t minValue, uint16_t maxValue) {
        return addUInt16Raw(static_cast<uint16_t>(id), static_cast<uint8_t>(group), path, field, minValue, maxValue);
    }
    template <typename Id, typename Group>
    bool addUInt32(Id id, Group group, const char* path, uint32_t* field, uint32_t minValue, uint32_t maxValue) {
        return addUInt32Raw(static_cast<uint16_t>(id), static_cast<uint8_t>(group), path, field, minValue, maxValue);
    }
    template <typename Id, typename Group>
    bool addFloat(Id id, Group group, const char* path, float* field, float minValue, float maxValue) {
        return addFloatRaw(static_cast<uint16_t>(id), static_cast<uint8_t>(group), path, field, minValue, maxValue);
    }
    template <typename Id, typename Group>
    bool addBool(Id id, Group group, const char* path, bool* field) {
        return addBoolRaw(static_cast<uint16_t>(id), static_cast<uint8_t>(group), path, field);
    }

    void setGroupNamer(GroupNamer namer) { _groupNamer = namer; }
    const char* groupName(uint8_t group) const;

    size_t count() const { return _count; }
    // The i-th binding in registration order (i < count()).
    const ParamBinding& at(size_t index) const { return _bindings[index]; }
    const ParamBinding* find(const char* path) const;
    // The bound field's current value as a number (bool as 0/1), for
    // snapshots and comparisons by the app.
    double numericValue(const ParamBinding& binding) const;

    // Parses valueText per the binding's type and writes the bound field on
    // success. Returns why it was rejected otherwise. Never touches fields
    // outside this binding. Unsigned types reject a sign ("-1" used to wrap)
    // and values past uint32.
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
    GroupNamer _groupNamer = nullptr;
};

}  // namespace param
