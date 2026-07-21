#pragma once

#include <cstdint>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// One entry from HarvesterObjectBaseline7's hopper list (Core3's HopperList,
// a DeltaVector<ResourceContainer*> subclass). Baseline entries carry no
// per-entry tag (see WaypointEntry.h's identical precedent) - only the delta
// path (not yet implemented here, no real captured delta traffic exists to
// verify against) would use HarvesterObjectDelta7.h's per-entry ADD/SET/
// REMOVE/CLEAR tags.
struct HarvesterHopperEntry {
    uint64_t resourceSpawnId = 0;
    float quantity = 0.0f;
};

// Parses the tagless baseline shape: int32 count, int32 updateCounter, then
// `count` entries of {uint64 resourceSpawnId, float quantity}.
ParseResult<std::vector<HarvesterHopperEntry>> parseHarvesterHopperList(soe::PacketBuffer& buf);

} // namespace swgproto
