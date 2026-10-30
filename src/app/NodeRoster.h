#pragma once

#include <stddef.h>
#include <stdint.h>

/*
NodeRoster

The bench / trial boards by their eFuse MAC (ESP.getEfuseMac(), the value the
Node prints as `RB node_id=0x...`), with a name for the logs and the base
bin each one's hop train starts on (the third column; the Node takes it
modulo the bases a train fits under, 0..3 on the 8-bin band). Was the
rotation E1=0 ... E5=4 on 5 bins (step 5g, owner 2026-10-17); since the
8-bin band (2026-10-30) four bases serve five boards: E5 takes 1, shared
with E2, the Analyzer, so the three bench Nodes E1 / E4 / E5 stay apart.
A board not in the table falls back to the MAC and prints name=unknown.
A stored output.hop_rotation wins over this table (PAR-010). A swapped ESP32
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
    {0x38FE4AC3DC24ULL, "E5", 1},  // 24:dc:c3:4a:fe:38
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
