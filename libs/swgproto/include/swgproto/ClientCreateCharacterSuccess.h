#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"

namespace swgproto {

// A hardcoded constant in Core3's own source (not a computed String::hashCode).
constexpr uint32_t kClientCreateCharacterSuccessHash = 0x1DB575CC;

struct ClientCreateCharacterSuccess {
    uint64_t objectId = 0; // the newly created character's object ID

    // Parses the fields following opCount+hash. Wire layout: uint64 objid.
    static ClientCreateCharacterSuccess parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
