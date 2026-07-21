#pragma once

#include <cstdint>

#include "soe/MessageHash.h"
#include "soe/PacketBuffer.h"

namespace swgproto {

// hashCode("ClientPermissionsMessage") - a computed message hash.
constexpr uint32_t kClientPermissionsMessageHash = soe::MessageHash::compute("ClientPermissionsMessage");

struct ClientPermissionsMessage {
    bool canConnect = false;
    bool canCreateCharacter = false;
    bool ignoresCharCreationMax = false; // possibly Jedi slot, per Core3's own comment
    bool unknown = false;

    // Parses the fields following opCount+hash. Wire layout: 4 booleans (1
    // byte each) - canConnect (galaxy available), canCreateCharacter,
    // ignoresCharCreationMax, unknown.
    static ClientPermissionsMessage parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
