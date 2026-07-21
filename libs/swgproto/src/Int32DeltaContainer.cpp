#include "swgproto/Int32DeltaContainer.h"

#include <exception>
#include <string>

namespace swgproto {

ParseResult<std::vector<int32_t>> parseInt32DeltaContainer(soe::PacketBuffer& buf) {
    uint32_t count = 0;

    try {
        count = buf.readUint32();
        buf.readUint32(); // updateCounter - discarded, see header comment
    } catch (const std::exception& e) {
        return ParseResult<std::vector<int32_t>>::invalid(
            std::string("buffer too short for container header: ") + e.what());
    }

    if (count > buf.remaining() / sizeof(int32_t)) {
        return ParseResult<std::vector<int32_t>>::invalid(
            "container count " + std::to_string(count) + " exceeds remaining buffer");
    }

    std::vector<int32_t> items;
    items.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        items.push_back(static_cast<int32_t>(buf.readUint32()));
    }

    return ParseResult<std::vector<int32_t>>::ok(std::move(items));
}

} // namespace swgproto
