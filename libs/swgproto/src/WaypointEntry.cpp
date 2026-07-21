#include "swgproto/WaypointEntry.h"

#include <exception>
#include <string>

namespace swgproto {

ParseResult<std::vector<WaypointEntry>> parseWaypointList(soe::PacketBuffer& buf) {
    uint32_t count = 0;

    try {
        count = buf.readUint32();
        buf.readUint32(); // updateCounter - discarded
    } catch (const std::exception& e) {
        return ParseResult<std::vector<WaypointEntry>>::invalid(
            std::string("buffer too short for map header: ") + e.what());
    }

    // Each entry is at least 1 (ADD tag) + 8 (key) + 4+4+4+4+8+4 (cellId..planetCrc)
    // + 4 (empty unicode name) + 8 (objectId) + 1 + 1 (color/active) = 51 bytes - a
    // cheap lower bound to reject an obviously corrupt count before allocating.
    if (count > buf.remaining() / 51) {
        return ParseResult<std::vector<WaypointEntry>>::invalid(
            "waypoint count " + std::to_string(count) + " exceeds remaining buffer");
    }

    std::vector<WaypointEntry> entries;
    entries.reserve(count);

    try {
        for (uint32_t i = 0; i < count; ++i) {
            uint8_t tag = buf.readByte();
            if (tag != 0x00) {
                return ParseResult<std::vector<WaypointEntry>>::invalid(
                    "entry " + std::to_string(i) + ": expected ADD tag (0x00), got " +
                    std::to_string(static_cast<int>(tag)));
            }
            WaypointEntry entry;
            entry.key = buf.readUint64();
            entry.cellId = static_cast<int32_t>(buf.readUint32());
            entry.x = buf.readFloat();
            entry.z = buf.readFloat();
            entry.y = buf.readFloat();
            entry.unknown = static_cast<int64_t>(buf.readUint64());
            entry.planetCrc = static_cast<int32_t>(buf.readUint32());
            entry.customName = buf.readUnicode();
            entry.objectId = static_cast<int64_t>(buf.readUint64());
            entry.color = buf.readByte();
            entry.active = buf.readByte();
            entries.push_back(std::move(entry));
        }
    } catch (const std::exception& e) {
        return ParseResult<std::vector<WaypointEntry>>::invalid(
            std::string("buffer too short reading waypoint entries: ") + e.what());
    }

    return ParseResult<std::vector<WaypointEntry>>::ok(std::move(entries));
}

} // namespace swgproto
