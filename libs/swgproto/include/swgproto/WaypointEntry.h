#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// One entry from PlayerObject's waypointList
// (DeltaVectorMap<uint64, WaypointObject>). `key` is the map's own key
// (the waypoint's object ID, per Core3's WaypointList.h); `objectId` is the
// SAME value written again as part of WaypointObject's own payload
// (WaypointObjectImplementation::insertToMessage) - confirmed redundant on
// the wire by reading source directly, represented faithfully here rather
// than assumed identical and collapsed into one field.
struct WaypointEntry {
    uint64_t key = 0;
    int32_t cellId = 0;
    float x = 0.0f;
    float z = 0.0f;
    float y = 0.0f;
    int64_t unknown = 0; // unused/reserved per source's own naming
    int32_t planetCrc = 0;
    std::u16string customName;
    int64_t objectId = 0; // redundant with `key`, see above
    uint8_t color = 0;
    uint8_t active = 0;
};

// Parses DeltaVectorMap<uint64, WaypointObject>::insertToMessage's shape:
// int32 size + int32 updateCounter, then per entry: byte(0x00 ADD tag) +
// uint64 key + WaypointObjectImplementation::insertToMessage's 10-field
// payload (cellId, x, z, y, unknown, planetCrc, customName, objectId,
// color, active - in that exact order, confirmed from source).
ParseResult<std::vector<WaypointEntry>> parseWaypointList(soe::PacketBuffer& buf);

} // namespace swgproto
