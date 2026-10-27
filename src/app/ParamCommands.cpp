#include "ParamCommands.h"
#include "SerialLine.h"

#include <Arduino.h>
#include <string.h>

#include "TextParse.h"

namespace param {

namespace {

constexpr size_t kMaxLineLength = serial_line::kCommandLineMax;

void printUsage(Print& out, bool hasStore) {
    out.println(hasStore ? "PARAM usage=PARAM LIST|GET <path>|SET <path> <value>|DUMP|SAVE|LOAD|RESET"
                         : "PARAM usage=PARAM LIST|GET <path>|SET <path> <value>|DUMP|RESET");
}

void printUnknownPath(Print& out, const char* verb, const char* path) {
    out.print("PARAM ");
    out.print(verb);
    out.print(" path=");
    out.print(path != nullptr ? path : "");
    out.print(" status=");
    out.println(paramSetStatusName(ParamSetStatus::UnknownParam));
}

}  // namespace

ParamLoadReport loadStoredParams(const ParamRegistry& registry, ParamStore& store, Print& out) {
    const ParamLoadReport report = registry.load(store, out);
    out.print("PARAM load stored=");
    out.print(static_cast<unsigned long>(report.stored));
    out.print(" applied=");
    out.print(static_cast<unsigned long>(report.applied));
    out.print(" stale=");
    out.print(static_cast<unsigned long>(report.stale));
    out.print(" invalid=");
    out.println(static_cast<unsigned long>(report.invalid));
    return report;
}

ParamCommandResult handleParamCommand(const char* line, const ParamRegistry& registry, ParamStore* store, Print& out) {
    ParamCommandResult result;
    char buffer[kMaxLineLength + 1];
    strncpy(buffer, line, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    char* savePtr = nullptr;
    char* token = strtok_r(buffer, " ", &savePtr);  // PARAM
    token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;
    if (token == nullptr) {
        printUsage(out, store != nullptr);
        return result;
    }

    if (text_parse::equalsIgnoreCase(token, "LIST")) {
        registry.list(out);
        return result;
    }

    if (text_parse::equalsIgnoreCase(token, "DUMP")) {
        registry.dump(out);
        return result;
    }

    // Persistence (PAR-011): only SAVE writes flash. LOAD re-reads the store,
    // RESET clears it and restores the firmware defaults; the caller applies
    // every module after both.
    if (text_parse::equalsIgnoreCase(token, "SAVE")) {
        if (store == nullptr) {
            out.println("PARAM SAVE status=no_store");
            return result;
        }
        const ParamSaveReport report = registry.save(*store);
        out.print("PARAM SAVE written=");
        out.print(static_cast<unsigned long>(report.written));
        out.print(" unchanged=");
        out.print(static_cast<unsigned long>(report.unchanged));
        out.print(" failed=");
        out.print(static_cast<unsigned long>(report.failed));
        out.print(" status=");
        out.println(report.failed == 0 ? "ok" : "error");
        return result;
    }

    if (text_parse::equalsIgnoreCase(token, "LOAD")) {
        if (store == nullptr) {
            out.println("PARAM LOAD status=no_store");
            return result;
        }
        loadStoredParams(registry, *store, out);
        result.effect = ParamCommandEffect::AllGroups;
        return result;
    }

    if (text_parse::equalsIgnoreCase(token, "RESET")) {
        const bool cleared = store != nullptr && store->clear();
        registry.restoreDefaults();
        out.print("PARAM RESET store=");
        out.print(store == nullptr ? "none" : (cleared ? "cleared" : "error"));
        out.println(" values=defaults");
        result.effect = ParamCommandEffect::AllGroups;
        return result;
    }

    if (text_parse::equalsIgnoreCase(token, "GET")) {
        const char* path = strtok_r(nullptr, " ", &savePtr);
        const ParamBinding* binding = path != nullptr ? registry.find(path) : nullptr;
        if (binding == nullptr) {
            printUnknownPath(out, "GET", path);
            return result;
        }
        out.print("PARAM GET path=");
        out.print(path);
        out.print(" module=");
        out.print(registry.groupName(binding->group));
        out.print(" value=");
        registry.printValue(out, *binding);
        out.println();
        return result;
    }

    if (text_parse::equalsIgnoreCase(token, "SET")) {
        const char* path = strtok_r(nullptr, " ", &savePtr);
        const char* value = strtok_r(nullptr, " ", &savePtr);
        // One value only: "1.5 junk" is an error, not 1.5 (issue #31 R2-19).
        const char* extra = strtok_r(nullptr, " ", &savePtr);
        if (extra != nullptr) {
            value = nullptr;
        }
        const ParamBinding* binding = path != nullptr ? registry.find(path) : nullptr;
        if (binding == nullptr) {
            printUnknownPath(out, "SET", path);
            return result;
        }

        out.print("PARAM SET path=");
        out.print(path);
        out.print(" module=");
        out.print(registry.groupName(binding->group));
        out.print(" old=");
        registry.printValue(out, *binding);

        const ParamSetStatus status = registry.applyValue(*binding, value);

        out.print(" new=");
        registry.printValue(out, *binding);
        out.print(" status=");
        out.println(paramSetStatusName(status));

        if (status == ParamSetStatus::Ok) {
            result.effect = ParamCommandEffect::Group;
            result.group = binding->group;
        }
        return result;
    }

    if (strchr(token, '=') != nullptr) {
        // The Analyzer's key=value grammar, removed in step 11e (decisions/
        // 2026-10-25-analyzer-params-on-registry-and-line-policy.md).
        out.println("ERR PARAM unknown; use PARAM SET detection.scalar_* <value>");
        return result;
    }

    printUsage(out, store != nullptr);
    return result;
}

}  // namespace param
