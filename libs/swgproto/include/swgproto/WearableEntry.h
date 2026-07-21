#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// One entry from CreatureObject's wearables list
// (WearablesDeltaVector, a DeltaVector<ManagedReference<TangibleObject*>>
// subclass under server/zone/objects/creature/variables/
// WearablesDeltaVector.h). Confirmed NOT the plain
// "ManagedReference -> uint64 ID" shape another DeltaVector<ManagedReference>
// field might use - WearablesDeltaVector overrides insertItemToMessage()
// with a genuinely richer 4-field payload per entry (customizationString,
// containmentType, objectId, clientObjectCRC), confirmed field-by-field
// against a real captured baseline (9 real equipped items, zero leftover
// bytes) before implementing - see CreatureObjectBaseline6.h's own comment.
struct WearableEntry {
    std::string customizationString; // can contain non-printable bytes (e.g. dye data)
    int32_t containmentType = 0;     // per source's own comment: "Equipped"
    uint64_t objectId = 0;
    int32_t clientObjectCrc = 0;
};

// Parses WearablesDeltaVector::insertToMessage's shape: uint32 count +
// uint32 updateCounter (from the base DeltaVector, unchanged), then per
// entry the override above - NO per-entry ADD tag byte (unlike
// DeltaVectorMap-based lists such as WaypointEntry's), since this is a
// plain DeltaVector, not a map.
ParseResult<std::vector<WearableEntry>> parseWearableList(soe::PacketBuffer& buf);

} // namespace swgproto
