#pragma once

#include <cstdint>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// One entry from CreatureObject's spaceMissionObjects
// (DeltaSet<uint64,uint64>, missionOwnerId -> missionObjectId). Unlike every
// DeltaVectorMap-based container in this project, CreatureObjectMessage4.h's
// baseline write is hand-rolled (NOT DeltaSet::insertToMessage) - it inserts
// the key/value pair directly with no per-entry ADD tag byte.
struct MissionObjectEntry {
    uint64_t missionOwnerId = 0;
    uint64_t missionObjectId = 0;
};

// Parses CreatureObjectMessage4.h's hand-rolled shape (confirmed directly
// from source, lines 49-63): int32 size + int32 updateCounter, then per
// entry: uint64 missionOwnerId + uint64 missionObjectId, no tag.
ParseResult<std::vector<MissionObjectEntry>> parseMissionObjectList(soe::PacketBuffer& buf);

} // namespace swgproto
