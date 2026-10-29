#pragma once

#include <Preferences.h>
#include <stdio.h>

#include <ParamRegistry.h>

/*
The Node's ParamStore (PAR-010): one NVS blob per param id in the namespace
"params", key "p<id>" (NVS keys are at most 15 characters, so the public paths
cannot be keys; the ids are stable and never reused, app/ParamIds.h). Written
only by PARAM SAVE (decisions/2026-10-23-param-persistence-save-only.md).
Owns: the NVS namespace "params" and its key scheme.
Does not: decide when to write (PARAM SAVE does) or what a param means (ParamRegistry).
*/
class NvsParamStore : public param::ParamStore {
public:
    bool read(uint16_t id, param::StoredParam& out) override {
        if (!open()) {
            return false;
        }
        char key[8];
        makeKey(id, key);
        if (!_prefs.isKey(key)) {
            return false;
        }
        return _prefs.getBytes(key, &out, sizeof(out)) == sizeof(out);
    }

    bool write(const param::StoredParam& entry) override {
        if (!open()) {
            return false;
        }
        char key[8];
        makeKey(entry.id, key);
        return _prefs.putBytes(key, &entry, sizeof(entry)) == sizeof(entry);
    }

    bool clear() override { return open() && _prefs.clear(); }

private:
    static void makeKey(uint16_t id, char (&key)[8]) { snprintf(key, sizeof(key), "p%u", static_cast<unsigned>(id)); }

    bool open() {
        if (!_open) {
            _open = _prefs.begin("params", false);
        }
        return _open;
    }

    Preferences _prefs;
    bool _open = false;
};
