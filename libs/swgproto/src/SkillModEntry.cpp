#include "swgproto/SkillModEntry.h"

#include <exception>
#include <string>

namespace swgproto {

ParseResult<std::vector<SkillModEntry>> parseSkillModList(soe::PacketBuffer& buf) {
    uint32_t count = 0;

    try {
        count = buf.readUint32();
        buf.readUint32(); // updateCounter - discarded
    } catch (const std::exception& e) {
        return ParseResult<std::vector<SkillModEntry>>::invalid(
            std::string("buffer too short for map header: ") + e.what());
    }

    // Each entry is at least 1 (ADD tag) + 2 (ASCII length prefix) + 4 + 4
    // (two int32 values) = 11 bytes - a cheap lower bound to reject an
    // obviously corrupt count before allocating.
    if (count > buf.remaining() / 11) {
        return ParseResult<std::vector<SkillModEntry>>::invalid(
            "map count " + std::to_string(count) + " exceeds remaining buffer");
    }

    std::vector<SkillModEntry> entries;
    entries.reserve(count);

    try {
        for (uint32_t i = 0; i < count; ++i) {
            uint8_t tag = buf.readByte();
            if (tag != 0x00) {
                return ParseResult<std::vector<SkillModEntry>>::invalid(
                    "entry " + std::to_string(i) + ": expected ADD tag (0x00), got " +
                    std::to_string(static_cast<int>(tag)));
            }
            SkillModEntry entry;
            entry.key = buf.readAscii();
            entry.skillMod = static_cast<int32_t>(buf.readUint32());
            entry.skillBonus = static_cast<int32_t>(buf.readUint32());
            entries.push_back(std::move(entry));
        }
    } catch (const std::exception& e) {
        return ParseResult<std::vector<SkillModEntry>>::invalid(
            std::string("buffer too short reading map entries: ") + e.what());
    }

    return ParseResult<std::vector<SkillModEntry>>::ok(std::move(entries));
}

} // namespace swgproto
