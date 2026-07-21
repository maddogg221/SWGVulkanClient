#include "swgproto/StringInt32Map.h"

#include <exception>
#include <string>

namespace swgproto {

ParseResult<std::vector<StringInt32Entry>> parseStringInt32Map(soe::PacketBuffer& buf) {
    uint32_t count = 0;

    try {
        count = buf.readUint32();
        buf.readUint32(); // updateCounter - discarded
    } catch (const std::exception& e) {
        return ParseResult<std::vector<StringInt32Entry>>::invalid(
            std::string("buffer too short for map header: ") + e.what());
    }

    // Each entry is at least 1 (ADD tag) + 2 (ASCII length prefix) + 4
    // (int32 value) = 7 bytes - a cheap lower bound to reject an obviously
    // corrupt count before allocating, matching this project's established
    // wire-supplied-count validation convention.
    if (count > buf.remaining() / 7) {
        return ParseResult<std::vector<StringInt32Entry>>::invalid(
            "map count " + std::to_string(count) + " exceeds remaining buffer");
    }

    std::vector<StringInt32Entry> entries;
    entries.reserve(count);

    try {
        for (uint32_t i = 0; i < count; ++i) {
            uint8_t tag = buf.readByte();
            if (tag != 0x00) {
                return ParseResult<std::vector<StringInt32Entry>>::invalid(
                    "entry " + std::to_string(i) + ": expected ADD tag (0x00), got " +
                    std::to_string(static_cast<int>(tag)));
            }
            StringInt32Entry entry;
            entry.key = buf.readAscii();
            entry.value = static_cast<int32_t>(buf.readUint32());
            entries.push_back(std::move(entry));
        }
    } catch (const std::exception& e) {
        return ParseResult<std::vector<StringInt32Entry>>::invalid(
            std::string("buffer too short reading map entries: ") + e.what());
    }

    return ParseResult<std::vector<StringInt32Entry>>::ok(std::move(entries));
}

} // namespace swgproto
