#include "swgproto/GroupObjectEntry.h"

#include <exception>

namespace swgproto {

ParseResult<std::vector<GroupMemberEntry>> parseGroupMemberList(soe::PacketBuffer& buf) {
    uint32_t count = 0;
    try {
        count = buf.readUint32();
        buf.readUint32(); // updateCounter - discarded
    } catch (const std::exception& e) {
        return ParseResult<std::vector<GroupMemberEntry>>::invalid(
            std::string("buffer too short for member list header: ") + e.what());
    }

    // Cheap lower bound (8 id + 2 empty-name-length) before allocating.
    if (count > buf.remaining() / 10) {
        return ParseResult<std::vector<GroupMemberEntry>>::invalid(
            "member count " + std::to_string(count) + " exceeds remaining buffer");
    }

    std::vector<GroupMemberEntry> entries;
    entries.reserve(count);
    try {
        for (uint32_t i = 0; i < count; ++i) {
            GroupMemberEntry entry;
            entry.objectId = buf.readUint64();
            entry.name = buf.readAscii();
            entries.push_back(std::move(entry));
        }
    } catch (const std::exception& e) {
        return ParseResult<std::vector<GroupMemberEntry>>::invalid(
            std::string("buffer too short reading member entries: ") + e.what());
    }

    return ParseResult<std::vector<GroupMemberEntry>>::ok(std::move(entries));
}

ParseResult<std::vector<GroupShipEntry>> parseGroupShipList(soe::PacketBuffer& buf) {
    uint32_t count = 0;
    try {
        count = buf.readUint32();
        buf.readUint32(); // updateCounter (reused from the member list on the wire) - discarded
    } catch (const std::exception& e) {
        return ParseResult<std::vector<GroupShipEntry>>::invalid(
            std::string("buffer too short for ship list header: ") + e.what());
    }

    if (count > buf.remaining() / 12) {
        return ParseResult<std::vector<GroupShipEntry>>::invalid(
            "ship count " + std::to_string(count) + " exceeds remaining buffer");
    }

    std::vector<GroupShipEntry> entries;
    entries.reserve(count);
    try {
        for (uint32_t i = 0; i < count; ++i) {
            GroupShipEntry entry;
            entry.shipId = buf.readUint64();
            entry.index = static_cast<int32_t>(buf.readUint32());
            entries.push_back(std::move(entry));
        }
    } catch (const std::exception& e) {
        return ParseResult<std::vector<GroupShipEntry>>::invalid(
            std::string("buffer too short reading ship entries: ") + e.what());
    }

    return ParseResult<std::vector<GroupShipEntry>>::ok(std::move(entries));
}

} // namespace swgproto
