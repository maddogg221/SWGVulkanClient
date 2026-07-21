#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"

namespace swgproto {

// A hardcoded constant in Core3's own source (LoginClusterStatus.h inserts
// this literal, not a computed String::hashCode) - do not try to derive it.
constexpr uint32_t kLoginClusterStatusHash = 0x3436AEB6;

struct GalaxyStatus {
    uint32_t galaxyId = 0;
    std::string address; // zone server IP, as a string
    uint16_t zonePort = 0;
    uint16_t pingPort = 0;
    uint32_t population = 0;
    uint32_t status = 0;
};

struct LoginClusterStatus {
    std::vector<GalaxyStatus> galaxies;

    // Parses the fields following opCount+hash. Wire layout: uint32
    // galaxyCount, then per galaxy: uint32 galaxyId + ASCII address +
    // uint16 zonePort + uint16 pingPort + uint32 population + uint32
    // (constant 0x00000CB2) + uint32 (constant 0x00000008) + uint32
    // (constant 0xFFFF8F80) + uint32 status + byte (constant 0).
    // IMPORTANT: zone/ping ports are per-galaxy and must be read here, not
    // assumed to be the conventional 44463/44462 - a real Finalizer capture
    // showed different ports per galaxy on the same host.
    static LoginClusterStatus parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
