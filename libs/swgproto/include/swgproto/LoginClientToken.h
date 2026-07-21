#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"

namespace swgproto {

// hashCode("LoginClientToken") - a computed message hash (STRING_HASHCODE in
// Core3's own source), not a hardcoded constant. Verified 0xAAB296C6 against
// a real captured Finalizer response.
constexpr uint32_t kLoginClientTokenHash = 0xAAB296C6;

struct LoginClientToken {
    std::vector<uint8_t> sessionToken;
    uint32_t accountId = 0;
    uint32_t stationId = 0;
    std::string username; // "Station Account Name" - may differ from the login handle

    // Parses the fields following opCount+hash (caller has already consumed
    // those). Wire layout (see DISCOVERY.txt): uint32 (sessionToken.length()+4)
    // + raw sessionToken bytes + uint32 accountId + uint32 stationId +
    // ASCII username.
    static LoginClientToken parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
