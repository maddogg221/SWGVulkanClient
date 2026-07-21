#include "swgproto/ByteDeltaContainer.h"

#include <exception>
#include <string>

namespace swgproto {

ParseResult<std::vector<uint8_t>> parseByteDeltaContainer(soe::PacketBuffer& buf) {
    uint32_t count = 0;

    try {
        count = buf.readUint32();
        buf.readUint32(); // updateCounter - discarded, see Int32DeltaContainer.h's
                           // identical convention
    } catch (const std::exception& e) {
        return ParseResult<std::vector<uint8_t>>::invalid(
            std::string("buffer too short for container header: ") + e.what());
    }

    if (count > buf.remaining()) {
        return ParseResult<std::vector<uint8_t>>::invalid(
            "container count " + std::to_string(count) + " exceeds remaining buffer");
    }

    return ParseResult<std::vector<uint8_t>>::ok(buf.readBytes(count));
}

} // namespace swgproto
