#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"

namespace swgproto {

// header2 for this sub-message - confirmed via Core3's SpatialChat.h:
// ObjectControllerMessage(targetID, 0x0B, 0xF4).
constexpr uint32_t kSpatialChatControllerType = 0xF4;

// Real spoken chat (/say, /shout, /whisper's spatial variant), triggered
// client-side via the "spatialchatinternal" QueueCommand
// (ChatManagerImplementation::handleSpatialChatInternalMessage), then
// broadcast to everyone in range. Confirmed live (zero leftover bytes) via
// a real /spatialchatinternal command and its resulting broadcast.
//
// Core3's SpatialChat.h has THREE overloaded constructors - this struct
// matches the raw-UnicodeString-message variant (confirmed live). A second
// variant builds the same `message` field from "@file:stringid" instead of
// player-typed text (a system/NPC-spoken message) - same wire shape from
// this parser's perspective, no separate handling needed. A THIRD variant
// (StringIdChatParameter-based, `insertInt(0)` instead of `insertUnicode`)
// has a genuinely different shape and is NOT confirmed live - not modeled
// here per this project's standing rule of only implementing confirmed
// traffic (see KNOWN_UNKNOWNS.md's "Core3 is a map, not truth" principle).
struct SpatialChat {
    uint64_t senderId = 0;
    uint64_t chatTargetId = 0;
    std::u16string message;
    uint16_t volume = 0;
    uint16_t spatialChatType = 0;
    uint16_t moodType = 0;
    uint8_t chatFlags = 0;
    uint8_t languageId = 0;
    // Trailing insertLong(0)/insertInt(0)/insertInt(0) from source - always
    // zero in the real capture, kept as real (if inert) fields for fidelity
    // rather than assumed and skipped.
    uint64_t unknownTrailingLong = 0;
    int32_t unknownTrailingInt1 = 0;
    int32_t unknownTrailingInt2 = 0;

    // Parses the fields following the shared ObjControllerMessage envelope
    // (header1/header2/objectId/unused already consumed).
    static SpatialChat parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
