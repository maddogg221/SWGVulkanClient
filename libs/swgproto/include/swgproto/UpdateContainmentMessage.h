#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"

namespace swgproto {

// Hardcoded literal in Core3's own UpdateContainmentMessage.h - confirmed
// in Phase 2 step 1 to also equal hashCode("UpdateContainmentMessage").
constexpr uint32_t kUpdateContainmentMessageHash = 0x56CBDE9E;

struct UpdateContainmentMessage {
    uint64_t objectId = 0;
    uint64_t containerId = 0; // 0 means "removed from its container/parent"
    int32_t type = 0;

    // Parses the fields following opCount+hash. Wire layout: uint64
    // objectId + uint64 containerId + int32 type.
    static UpdateContainmentMessage parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
