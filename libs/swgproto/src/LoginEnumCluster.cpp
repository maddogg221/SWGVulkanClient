#include "swgproto/LoginEnumCluster.h"

#include <stdexcept>

namespace swgproto {

namespace {
// galaxyId(4) + ASCII name length prefix(2, 0 chars minimum) + constant(4).
constexpr size_t kMinGalaxyEntrySize = 10;
} // namespace

LoginEnumCluster LoginEnumCluster::parse(soe::PacketBuffer& buf) {
    LoginEnumCluster result;

    uint32_t galaxyCount = buf.readUint32();
    if (galaxyCount > buf.remaining() / kMinGalaxyEntrySize) {
        throw std::runtime_error("LoginEnumCluster: galaxyCount exceeds available data");
    }
    result.galaxies.reserve(galaxyCount);

    for (uint32_t i = 0; i < galaxyCount; ++i) {
        GalaxyEntry entry;
        entry.galaxyId = buf.readUint32();
        entry.name = buf.readAscii();
        buf.readUint32(); // constant 0xFFFF8F80, unused
        result.galaxies.push_back(std::move(entry));
    }

    buf.readUint32(); // finish terminator, constant 0x00000008, unused

    return result;
}

} // namespace swgproto
