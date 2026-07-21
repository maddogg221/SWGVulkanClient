#include "swgproto/WearableEntry.h"

#include <exception>
#include <string>

namespace swgproto {

ParseResult<std::vector<WearableEntry>> parseWearableList(soe::PacketBuffer& buf) {
    uint32_t count = 0;

    try {
        count = buf.readUint32();
        buf.readUint32(); // updateCounter - discarded
    } catch (const std::exception& e) {
        return ParseResult<std::vector<WearableEntry>>::invalid(
            std::string("buffer too short for container header: ") + e.what());
    }

    // Each entry is at least 2 (empty ascii len) + 4 (containmentType) + 8
    // (objectId) + 4 (crc) = 18 bytes - a cheap lower bound to reject an
    // obviously corrupt count before allocating.
    if (count > buf.remaining() / 18) {
        return ParseResult<std::vector<WearableEntry>>::invalid(
            "wearables count " + std::to_string(count) + " exceeds remaining buffer");
    }

    std::vector<WearableEntry> entries;
    entries.reserve(count);

    try {
        for (uint32_t i = 0; i < count; ++i) {
            WearableEntry entry;
            entry.customizationString = buf.readAscii();
            entry.containmentType = static_cast<int32_t>(buf.readUint32());
            entry.objectId = buf.readUint64();
            entry.clientObjectCrc = static_cast<int32_t>(buf.readUint32());
            entries.push_back(std::move(entry));
        }
    } catch (const std::exception& e) {
        return ParseResult<std::vector<WearableEntry>>::invalid(
            std::string("buffer too short reading wearable entries: ") + e.what());
    }

    return ParseResult<std::vector<WearableEntry>>::ok(std::move(entries));
}

} // namespace swgproto
