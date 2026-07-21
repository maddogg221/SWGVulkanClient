#include "swgproto/LoginClusterStatus.h"

#include <stdexcept>

namespace swgproto {

namespace {
// galaxyId(4) + ASCII address length prefix(2, 0 chars min) + zonePort(2) +
// pingPort(2) + population(4) + 3 constants(12) + status(4) + byte(1).
constexpr size_t kMinGalaxyStatusSize = 31;
} // namespace

LoginClusterStatus LoginClusterStatus::parse(soe::PacketBuffer& buf) {
    LoginClusterStatus result;

    uint32_t galaxyCount = buf.readUint32();
    if (galaxyCount > buf.remaining() / kMinGalaxyStatusSize) {
        throw std::runtime_error("LoginClusterStatus: galaxyCount exceeds available data");
    }
    result.galaxies.reserve(galaxyCount);

    for (uint32_t i = 0; i < galaxyCount; ++i) {
        GalaxyStatus status;
        status.galaxyId = buf.readUint32();
        status.address = buf.readAscii();
        status.zonePort = buf.readUint16();
        status.pingPort = buf.readUint16();
        status.population = buf.readUint32();
        buf.readUint32(); // constant 0x00000CB2, unused
        buf.readUint32(); // constant 0x00000008, unused
        buf.readUint32(); // constant 0xFFFF8F80, unused
        status.status = buf.readUint32();
        buf.readByte(); // constant 0, unused
        result.galaxies.push_back(std::move(status));
    }

    return result;
}

} // namespace swgproto
