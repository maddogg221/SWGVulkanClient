#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"

namespace swgproto {

// A hardcoded constant in Core3's own source (not a computed String::hashCode).
constexpr uint32_t kClientCreateCharacterFailedHash = 0xDF333C6E;

struct ClientCreateCharacterFailed {
    std::string errorString; // a specific error code, e.g. "name_declined_empty"

    // Parses the fields following opCount+hash. Wire layout: uint32 zero
    // (unused placeholder) + ASCII "ui" (STF file, unused) + uint32 zero
    // (spacer) + ASCII errorString.
    static ClientCreateCharacterFailed parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
