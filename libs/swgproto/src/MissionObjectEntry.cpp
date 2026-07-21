#include "swgproto/MissionObjectEntry.h"

#include <exception>
#include <string>

namespace swgproto {

ParseResult<std::vector<MissionObjectEntry>> parseMissionObjectList(soe::PacketBuffer& buf) {
    uint32_t count = 0;

    try {
        count = buf.readUint32();
        buf.readUint32(); // updateCounter - discarded
    } catch (const std::exception& e) {
        return ParseResult<std::vector<MissionObjectEntry>>::invalid(
            std::string("buffer too short for list header: ") + e.what());
    }

    // Each entry is 16 bytes (2 uint64s) - a cheap lower bound to reject an
    // obviously corrupt count before allocating.
    if (count > buf.remaining() / 16) {
        return ParseResult<std::vector<MissionObjectEntry>>::invalid(
            "list count " + std::to_string(count) + " exceeds remaining buffer");
    }

    std::vector<MissionObjectEntry> entries;
    entries.reserve(count);

    try {
        for (uint32_t i = 0; i < count; ++i) {
            MissionObjectEntry entry;
            entry.missionOwnerId = buf.readUint64();
            entry.missionObjectId = buf.readUint64();
            entries.push_back(entry);
        }
    } catch (const std::exception& e) {
        return ParseResult<std::vector<MissionObjectEntry>>::invalid(
            std::string("buffer too short reading list entries: ") + e.what());
    }

    return ParseResult<std::vector<MissionObjectEntry>>::ok(std::move(entries));
}

} // namespace swgproto
