#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/GroupObjectBaseline6.h"

namespace swgproto {

// Result of applying a GRUP6 DeltasMessage's `count` update entries directly
// onto an already-decoded GroupObjectBaseline6. Deliberately NOT built on
// SchemaEngine::applyDeltaMessage - see GroupObjectBaseline6.h's class
// comment for why this type's delta genuinely can't share that generic
// mechanism: indices 0x01 (members) and 0x02 (ships) carry Core3's own
// hand-rolled per-entry ADD=1/UPDATE=2/REMOVE=0/CLEAR=4 tag scheme,
// incrementally mutating a previously-stored list rather than replacing one
// field with one freshly-decoded value.
struct GroupObjectDeltaResult {
    std::vector<uint16_t> appliedFieldIndices;
    uint16_t totalCount = 0;
    bool stoppedEarly = false;
    std::string stopReason; // populated iff stoppedEarly
};

// Applies `count` field updates from `buf` onto `state` in place. Confirmed
// live indices, from both source (GroupObjectDeltaMessage6.h real call
// sites: addMember()/removeMember()/makeLeader()/updateMembers()/
// GroupManager::createGroup()->initialUpdate(), updateLootRules()) and a
// real captured delta (122 bytes, zero leftover, indices 0x01/0x02/0x03/
// 0x04/0x06/0x07 all present and decoding cleanly):
//   0x01 members (list mutation, see below)
//   0x02 ships (list mutation, same tag scheme, trailing int32 index repeated
//        per entry - always a full-list rewrite bundled with 0x01, no
//        standalone incremental ship-only delta exists in source)
//   0x03 groupName - uint16-len-prefixed ascii, always empty in practice
//   0x04 groupLevel - uint16
//   0x06 masterLooterId - uint64
//   0x07 lootRule - uint32
// 0x05 ("unknown int") is fully commented out in source, never compiled -
// not handled here; an unrecognized index (0x05 or anything else) stops
// decoding early, matching SchemaEngine::applyDeltaMessage's own convention.
//
// Members/ships list shape: int32 opCount, int32 updateCounter, then
// opCount entries of insertByte(tag) + insertShort(index) + payload:
//   ADD(1)/UPDATE(2): uint64 id + ascii name (members) or uint64 shipId +
//     int32 index (ships) - both tags carry the same payload shape and are
//     applied identically here (write the entry at `index`, growing the
//     vector if needed) - REMOVE and CLEAR are implemented per source
//     (GroupObjectDeltaMessage6.h's DeltaVector convention, real call sites
//     cited above) but have not yet been independently observed on the
//     wire - only ADD/UPDATE(tag 1) is traffic-confirmed so far.
//   REMOVE(0): uint16 index only, no payload - erases that index if valid.
//   CLEAR(4): no payload - clears the whole list.
GroupObjectDeltaResult applyGroupObjectBaseline6Delta(GroupObjectBaseline6& state, uint16_t count,
                                                       soe::PacketBuffer& buf);

} // namespace swgproto
