#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"

namespace swgproto {

// Hardcoded literal in Core3's own server-side class (named
// UpdatePVPStatusMessage there) - confirmed in Phase 2 step 1 to also equal
// hashCode("UpdatePvpStatusMessage"), the name this project uses.
constexpr uint32_t kUpdatePvpStatusMessageHash = 0x08A1C126;

struct UpdatePvpStatusMessage {
    uint32_t pvpStatusBitmask = 0;
    uint32_t faction = 0;
    uint64_t objectId = 0;

    // Parses the fields following opCount+hash. Wire layout: uint32
    // pvpStatusBitmask + uint32 faction + uint64 objectId.
    static UpdatePvpStatusMessage parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
