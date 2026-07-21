#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"

namespace swgproto {

// header2 for this sub-message - confirmed via Core3's Emote.h:
// ObjectControllerMessage(targetID, 0x0B, 0x12E).
constexpr uint32_t kEmoteControllerType = 0x12E;

// A social emote (e.g. "/wave"), triggered client-side via the
// "socialInternal" QueueCommand
// (ChatManagerImplementation::handleSocialInternalMessage), broadcast to
// nearby players (self always included regardless of range). Confirmed
// live 2026-07-18 by sending a real socialInternal command and precisely
// re-decoding the resulting broadcast (zero leftover bytes), matching
// Emote.h's constructor exactly:
// insertLong(senderID) + insertLong(emoteTargetID) + insertInt(emoteID) +
// insertByte((doAnim ? 0x01 : 0) | (doText ? 0x02 : 0)).
struct Emote {
    uint64_t senderId = 0;
    uint64_t emoteTargetId = 0;
    uint32_t emoteId = 0;
    uint8_t animTextFlags = 0;

    // Parses the fields following the shared ObjControllerMessage envelope
    // (header1/header2/objectId/unused already consumed).
    static Emote parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
