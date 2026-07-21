#pragma once

#include <cstdint>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// One entry from PlayerObject's schematics list (SchematicList extends
// DeltaVector<ManagedReference<DraftSchematic*>>). Core3's own
// SchematicList::insertToMessage writes each entry's getClientObjectCRC()
// TWICE, with a source comment ("Must be client CRC") flagging this as
// intentional rather than accidental duplication - modeled here as two
// separate fields rather than assumed identical, same faithfulness
// principle already used for WaypointEntry's redundant key/objectId.
struct SchematicEntry {
    int32_t crc = 0;
    int32_t crcDuplicate = 0;
};

// Parses SchematicList::insertToMessage's shape: int32 size + int32
// updateCounter, then per entry: int32 crc + int32 crcDuplicate (8 bytes,
// no ADD-tag byte unlike the DeltaVectorMap-based containers - this is a
// plain DeltaVector, not a map).
ParseResult<std::vector<SchematicEntry>> parseSchematicList(soe::PacketBuffer& buf);

} // namespace swgproto
