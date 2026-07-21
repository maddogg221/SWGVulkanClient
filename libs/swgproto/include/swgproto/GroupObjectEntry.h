#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// One entry from GroupObjectBaseline6's member list (Core3's own hand-rolled
// "DeltaVector", not the DeltaVectorMap convention used elsewhere in this
// project - see GroupObjectBaseline6.h's class comment). Baseline entries
// carry NO per-entry tag byte (insertMembers()'s list->insertToMessage()
// shape); the delta path (GroupObjectDelta6.h) reuses this same struct but
// reads a tag byte per entry.
struct GroupMemberEntry {
    uint64_t objectId = 0;
    std::string name; // ascii - BASE6 baseline uses getCustomObjectName(), the delta uses
                       // getDisplayedName() for the same conceptual field (a real,
                       // source-confirmed inconsistency, not a transcription risk to "fix")
};

// One entry from GroupObjectBaseline6's ship list. Confirmed from source: the
// BASELINE's ship-list header (count+updateCounter) is written fresh but
// populated from the MEMBER list's own size/counter variables, not the ship
// list's - a real Core3 quirk, not a transcription risk to "fix" (see
// GroupObjectBaseline6.h). `index` is repeated in the delta's per-entry
// payload but not in the baseline's.
struct GroupShipEntry {
    uint64_t shipId = 0; // 0 if the member isn't a player or has no ship
    int32_t index = 0;   // the corresponding member's list position
};

// Parses the tagless baseline shape shared by both lists: int32 count, int32
// updateCounter, then `count` entries of {uint64 objectId, ascii name} with
// no per-entry tag byte.
ParseResult<std::vector<GroupMemberEntry>> parseGroupMemberList(soe::PacketBuffer& buf);

// Same tagless shape, entries of {uint64 shipId, int32 index}.
ParseResult<std::vector<GroupShipEntry>> parseGroupShipList(soe::PacketBuffer& buf);

} // namespace swgproto
