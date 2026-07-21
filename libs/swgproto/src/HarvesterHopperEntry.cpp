#include "swgproto/HarvesterHopperEntry.h"

#include <exception>

namespace swgproto {

ParseResult<std::vector<HarvesterHopperEntry>> parseHarvesterHopperList(soe::PacketBuffer& buf) {
    uint32_t count = 0;
    try {
        count = buf.readUint32();
        buf.readUint32(); // updateCounter - discarded
    } catch (const std::exception& e) {
        return ParseResult<std::vector<HarvesterHopperEntry>>::invalid(
            std::string("buffer too short for hopper list header: ") + e.what());
    }

    if (count > buf.remaining() / 12) { // 8 (id) + 4 (float) per entry
        return ParseResult<std::vector<HarvesterHopperEntry>>::invalid(
            "hopper count " + std::to_string(count) + " exceeds remaining buffer");
    }

    std::vector<HarvesterHopperEntry> entries;
    entries.reserve(count);
    try {
        for (uint32_t i = 0; i < count; ++i) {
            HarvesterHopperEntry entry;
            entry.resourceSpawnId = buf.readUint64();
            entry.quantity = buf.readFloat();
            entries.push_back(entry);
        }
    } catch (const std::exception& e) {
        return ParseResult<std::vector<HarvesterHopperEntry>>::invalid(
            std::string("buffer too short reading hopper entries: ") + e.what());
    }

    return ParseResult<std::vector<HarvesterHopperEntry>>::ok(std::move(entries));
}

} // namespace swgproto
