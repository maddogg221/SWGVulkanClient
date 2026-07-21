#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"

namespace swgproto {

// A hardcoded constant in Core3's own source (LoginEnumCluster.h inserts
// this literal, not a computed String::hashCode) - do not try to derive it.
constexpr uint32_t kLoginEnumClusterHash = 0xC11C63B9;

struct GalaxyEntry {
    uint32_t galaxyId = 0;
    std::string name;
};

struct LoginEnumCluster {
    std::vector<GalaxyEntry> galaxies;

    // Parses the fields following opCount+hash. Wire layout: uint32
    // galaxyCount, then per galaxy: uint32 galaxyId + ASCII name + uint32
    // (constant 0xFFFF8F80), then a trailing uint32 "finish" terminator
    // (constant 0x00000008) after all galaxies.
    static LoginEnumCluster parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
