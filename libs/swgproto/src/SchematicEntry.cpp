#include "swgproto/SchematicEntry.h"

#include <exception>
#include <string>

namespace swgproto {

ParseResult<std::vector<SchematicEntry>> parseSchematicList(soe::PacketBuffer& buf) {
    uint32_t count = 0;

    try {
        count = buf.readUint32();
        buf.readUint32(); // updateCounter - discarded
    } catch (const std::exception& e) {
        return ParseResult<std::vector<SchematicEntry>>::invalid(
            std::string("buffer too short for list header: ") + e.what());
    }

    // Each entry is 8 bytes (crc + crcDuplicate, no ADD tag - this is a
    // plain DeltaVector, not a DeltaVectorMap).
    if (count > buf.remaining() / 8) {
        return ParseResult<std::vector<SchematicEntry>>::invalid(
            "list count " + std::to_string(count) + " exceeds remaining buffer");
    }

    std::vector<SchematicEntry> entries;
    entries.reserve(count);

    try {
        for (uint32_t i = 0; i < count; ++i) {
            SchematicEntry entry;
            entry.crc = static_cast<int32_t>(buf.readUint32());
            entry.crcDuplicate = static_cast<int32_t>(buf.readUint32());
            entries.push_back(entry);
        }
    } catch (const std::exception& e) {
        return ParseResult<std::vector<SchematicEntry>>::invalid(
            std::string("buffer too short reading list entries: ") + e.what());
    }

    return ParseResult<std::vector<SchematicEntry>>::ok(std::move(entries));
}

} // namespace swgproto
