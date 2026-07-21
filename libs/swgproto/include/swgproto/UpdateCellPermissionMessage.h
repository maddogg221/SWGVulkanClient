#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"

namespace swgproto {

// Hardcoded literal in Core3's own server-side class (named
// UpdateCellPermissionsMessage there, WITH an s - the wire hash matches
// hashCode("UpdateCellPermissionMessage"), WITHOUT one, per the reference
// client's own dispatch table used in Phase 2 step 1; kept without the s
// here to match this project's established naming for this message).
constexpr uint32_t kUpdateCellPermissionMessageHash = 0xF612499C;

struct UpdateCellPermissionMessage {
    bool allowEntry = false; // false denies entry, true allows it
    uint64_t cellObjectId = 0;

    // Parses the fields following opCount+hash. Wire layout: byte
    // allowEntry + uint64 cellObjectId.
    static UpdateCellPermissionMessage parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
