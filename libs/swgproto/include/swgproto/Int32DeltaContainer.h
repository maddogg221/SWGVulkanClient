#pragma once

#include <cstdint>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// Parses the "count + updateCounter + N plain int32 items" shape used by
// DeltaVector<int>/AutoDeltaSet<int> (Core3's DeltaVector.h/AutoDeltaSet.h:
// both implement insertToMessage() as uint32 count + uint32 updateCounter +
// count raw int32 items - confirmed identical in both files). The
// updateCounter itself is a server-side dirty-tracking counter, not
// meaningful to a fresh client, so it's read (to keep the cursor aligned)
// but discarded rather than returned.
//
// IMPORTANT: this is ONE of several "Delta*" container encodings Core3
// uses on the wire, not a universal one - the name/type are deliberately
// specific rather than calling this "the" container parser. SkillList (see
// CreatureObjectBaseline1.cpp) shares the same count+updateCounter header
// but writes non-null entries as ASCII strings instead of raw int32s, and
// skips null entries with no placeholder - it is NOT routed through this
// parser, and reusing this one for it would misread past the real data
// (this exact mistake was made and fixed once already during Phase 2 step
// 4's implementation - see SESSION_LOG.md). Other Delta* container types
// referenced during that step's source research (SkillModList, AbilityList,
// DeltaVectorMap, DeltaBitArray - used by CreatureObjectMessage4/6 and
// PlayerObjectMessage8/9, still out of scope) have not been read from
// source yet and likely need their own parsers too, not a wider version of
// this one - see PLAN.md step 5.
//
// A `count` that would overrun the buffer is reported as `invalid` rather
// than causing an out-of-range read, matching this project's established
// wire-supplied-count validation convention (see e.g.
// EnumerateCharacterId::parse).
ParseResult<std::vector<int32_t>> parseInt32DeltaContainer(soe::PacketBuffer& buf);

} // namespace swgproto
