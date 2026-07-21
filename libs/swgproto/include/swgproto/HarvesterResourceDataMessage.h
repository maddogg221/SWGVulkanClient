#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"

namespace swgproto {

// header2 for this sub-message - confirmed via Core3's
// HarvesterResourceDataMessage.h: ObjectControllerMessage(player->getObjectID(),
// 0x0B, 0xEA). Note the shared envelope's objectId field is the REQUESTING
// PLAYER's own id here, not the harvester's - the harvester's id is instead
// this message's own first field below.
constexpr uint32_t kHarvesterResourceDataMessageControllerType = 0xEA;

// One resource spawn available to a harvester, from
// HarvesterResourceDataMessage.h's insertResourceList(): insertLong(id) +
// insertAscii(name) + insertAscii(type) + insertByte((int)(density*100)).
struct HarvesterResourceDataEntry {
    uint64_t resourceSpawnId = 0;
    std::string name;
    std::string resourceType;
    uint8_t densityPercent = 0;

    static HarvesterResourceDataEntry parse(soe::PacketBuffer& buf);
};

// Reply to the real "harvesterGetResourceData" QueueCommand
// (HarvesterGetResourceDataCommand.h) - lists every resource spawn a
// harvester could select, with real-time density at its position. Traced
// and hand-decoded live during Phase 9's harvester activation
// investigation (zero leftover bytes across 3 real captures - see
// DISCOVERY.txt/KNOWN_UNKNOWNS.md) but never turned into an actual
// swgproto type at the time, since it wasn't that phase's target message.
// Implemented here from that same documented, real-traffic-confirmed
// shape (KNOWN_UNKNOWNS.md's original "u32 unknown(11), u32 unknown(234),
// u64 requestingPlayerId, u32 unknown(0)" fields were the raw
// ObjControllerMessage envelope itself, captured un-stripped via the
// generic unknown-hash dump path - 11 (0x0B) is header1, 234 (0xEA) is
// this message's own header2, requestingPlayerId is the envelope's
// objectId, and the trailing unknown(0) is the envelope's own unused
// field. Re-deriving this now that swgproto's convention always starts
// parsing AFTER that shared envelope resolves the apparent mismatch with
// this class's own source, which has no such fields of its own).
struct HarvesterResourceDataMessage {
    uint64_t harvesterObjectId = 0;
    std::vector<HarvesterResourceDataEntry> resources;

    // Parses the fields following the shared ObjControllerMessage envelope
    // (header1/header2/objectId/unused already consumed).
    static HarvesterResourceDataMessage parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
