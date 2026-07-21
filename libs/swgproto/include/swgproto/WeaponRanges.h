#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"

namespace swgproto {

// header2 for this sub-message - confirmed via Core3's WeaponRanges.h:
// ObjectControllerMessage(creo->getObjectID(), 0x1B, 0x140).
constexpr uint32_t kWeaponRangesControllerType = 0x140;

// Sent self-only whenever a creature's equipped weapon changes
// (CreatureObjectImplementation::setWeapon()) - including the default
// (unarmed) weapon assigned at character load, confirmed live: a plain
// zone-in produces two real instances (the default weapon, then the
// character's actual equipped weapon), not just during combat. The
// "892 real instances" observed in early Phase 4 exploration captures
// reflects how often setWeapon() itself gets called during real
// gameplay/combat, not a periodic/idle broadcast.
struct WeaponRanges {
    uint64_t weaponObjectId = 0;
    float idealRange = 0.0f;
    float maxRange = 0.0f;
    int32_t pointBlankAccuracy = 0;
    int32_t idealAccuracy = 0;
    int32_t maxRangeAccuracy = 0;

    // Parses the fields following the shared ObjControllerMessage envelope
    // (header1/header2/objectId/unused already consumed). Wire layout,
    // confirmed live (28 bytes, zero leftover, two real captures): long
    // weaponObjectId, float idealRange, float maxRange, int
    // pointBlankAccuracy, int idealAccuracy, int maxRangeAccuracy.
    static WeaponRanges parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
