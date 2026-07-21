#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"

namespace swgproto {

// A hardcoded constant in Core3's own source (not a computed String::hashCode).
constexpr uint32_t kClientRandomNameResponseHash = 0xE85FB868;

struct ClientRandomNameResponse {
    std::string raceIff;
    std::u16string name; // the server-suggested random name

    // Parses the fields following opCount+hash. Wire layout: ASCII raceIff
    // (echoed back) + Unicode name + ASCII "ui" (STF file, unused) + uint32
    // zero spacer + ASCII "name_approved" (STF variable name, unused).
    static ClientRandomNameResponse parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
