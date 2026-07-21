#pragma once

#include <cstdint>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// Parses the "count + updateCounter + N plain uint64 items" shape used by
// DeltaVector<ManagedReference<T*>> (Core3's DeltaVector.h::insertToMessage:
// uint32 count + uint32 updateCounter + count items, each written via
// TypeInfo<E>::toBinaryStream - for a ManagedReference<T*> this reduces to
// the referenced object's ID, an 8-byte value on the wire, matching every
// other object-ID-shaped field already confirmed in this codebase). Same
// header shape as Int32DeltaContainer (see that file's own comment for why
// this project keeps these container parsers separate per confirmed item
// type rather than one "universal" parser) - just 8-byte items instead of
// 4-byte ones. Used for CreatureObjectBaseline6's wearables list and
// TangibleObjectBaseline6's defenders list, both DeltaVector<ManagedReference<...>>.
//
// A `count` that would overrun the buffer is reported as `invalid` rather
// than causing an out-of-range read, matching this project's established
// wire-supplied-count validation convention.
ParseResult<std::vector<uint64_t>> parseUint64DeltaContainer(soe::PacketBuffer& buf);

} // namespace swgproto
