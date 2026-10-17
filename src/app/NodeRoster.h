#pragma once

#include <stddef.h>
#include <stdint.h>

/*
NodeRoster

The bench / trial boards by their eFuse MAC (ESP.getEfuseMac(), the value the
Node prints as `RB node_id=0x...`), with a name for the logs and the hop
rotation each one plays (step 5g, owner 2026-10-17: E1=0 ... E5=4). Before
this, the rotation was MAC % bins, which gave E4 and E5 the same start bin.
A board not in the table falls back to MAC % bins and prints name=unknown.
Nothing persists a PARAM SET output.hop_rotation across a reset (PAR-014 is
step 11), so this table is the trial's per-board config. A swapped ESP32
gets a new MAC: add its line here (and note it in the bench session).
*/
namespace roster {

struct Entry {
    uint64_t efuseMac;
    const char* name;
    uint8_t hopRotation;
};

constexpr Entry kNodes[] = {
    {0x50B04AC3DC24ULL, "E1", 0},  // 24:dc:c3:4a:b0:50
    {0xE84C4BC3DC24ULL, "E2", 1},  // 24:dc:c3:4b:4c:e8
    {0x38B749C3DC24ULL, "E3", 2},  // 24:dc:c3:49:b7:38
    {0xECEF4AC3DC24ULL, "E4", 3},  // 24:dc:c3:4a:ef:ec
    {0x38FE4AC3DC24ULL, "E5", 4},  // 24:dc:c3:4a:fe:38
};

inline const Entry* find(uint64_t efuseMac) {
    for (size_t i = 0; i < sizeof(kNodes) / sizeof(kNodes[0]); ++i) {
        if (kNodes[i].efuseMac == efuseMac) {
            return &kNodes[i];
        }
    }
    return nullptr;
}

}  // namespace roster
