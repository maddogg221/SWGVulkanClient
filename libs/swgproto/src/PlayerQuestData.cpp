#include "swgproto/PlayerQuestData.h"

#include <exception>
#include <string>

namespace swgproto {

ParseResult<std::vector<PlayerQuestEntry>> parsePlayerQuestDataMap(soe::PacketBuffer& buf) {
    uint32_t count = 0;

    try {
        count = buf.readUint32();
        buf.readUint32(); // updateCounter - discarded
    } catch (const std::exception& e) {
        return ParseResult<std::vector<PlayerQuestEntry>>::invalid(
            std::string("buffer too short for map header: ") + e.what());
    }

    // Each entry is at least 1 (ADD tag) + 4 (uint32 key) + 17 (PlayerQuestData) = 22
    // bytes - a cheap lower bound to reject an obviously corrupt count before
    // allocating.
    if (count > buf.remaining() / 22) {
        return ParseResult<std::vector<PlayerQuestEntry>>::invalid(
            "map count " + std::to_string(count) + " exceeds remaining buffer");
    }

    std::vector<PlayerQuestEntry> entries;
    entries.reserve(count);

    try {
        for (uint32_t i = 0; i < count; ++i) {
            uint8_t tag = buf.readByte();
            if (tag != 0x00) {
                return ParseResult<std::vector<PlayerQuestEntry>>::invalid(
                    "entry " + std::to_string(i) + ": expected ADD tag (0x00), got " +
                    std::to_string(static_cast<int>(tag)));
            }
            PlayerQuestEntry entry;
            entry.key = buf.readUint32();
            entry.value.ownerId = buf.readUint64();
            entry.value.activeStepBitmask = buf.readUint16();
            entry.value.completedStepBitmask = buf.readUint16();
            entry.value.completedFlag = buf.readByte();
            entry.value.questCounter = buf.readUint32();
            entries.push_back(std::move(entry));
        }
    } catch (const std::exception& e) {
        return ParseResult<std::vector<PlayerQuestEntry>>::invalid(
            std::string("buffer too short reading map entries: ") + e.what());
    }

    return ParseResult<std::vector<PlayerQuestEntry>>::ok(std::move(entries));
}

} // namespace swgproto
