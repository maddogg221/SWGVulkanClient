#include "swgproto/Uint64DeltaContainer.h"

#include <exception>
#include <string>

namespace swgproto {

ParseResult<std::vector<uint64_t>> parseUint64DeltaContainer(soe::PacketBuffer& buf) {
    uint32_t count = 0;

    try {
        count = buf.readUint32();
        buf.readUint32(); // updateCounter - discarded, see header comment
    } catch (const std::exception& e) {
        return ParseResult<std::vector<uint64_t>>::invalid(
            std::string("buffer too short for container header: ") + e.what());
    }

    if (count > buf.remaining() / sizeof(uint64_t)) {
        return ParseResult<std::vector<uint64_t>>::invalid(
            "container count " + std::to_string(count) + " exceeds remaining buffer");
    }

    std::vector<uint64_t> items;
    items.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        items.push_back(buf.readUint64());
    }

    return ParseResult<std::vector<uint64_t>>::ok(std::move(items));
}

} // namespace swgproto
