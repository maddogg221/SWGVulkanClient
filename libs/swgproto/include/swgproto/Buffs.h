#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"

namespace swgproto {

// header2 values - confirmed via Core3's Buffs.h:
// AddBuffMessage: ObjectControllerMessage(objid, 0x1B, 0x229, false).
// RemoveBuffMessage: ObjectControllerMessage(objid, 0x1B, 0x22A, false).
constexpr uint32_t kAddBuffMessageControllerType = 0x229;
constexpr uint32_t kRemoveBuffMessageControllerType = 0x22A;

// Adds a buff to the player's buff window (fires from any real buff grant -
// BuffImplementation.cpp is the sole real caller for the generic Buff
// system). Confirmed live 2026-07-18 via a real /burstrun command, zero
// leftover bytes, matching AddBuffMessage.h's constructor exactly:
// insertInt(buffcrc) + insertFloat(duration).
struct AddBuffMessage {
    uint32_t buffCrc = 0;
    float duration = 0.0f;

    // Parses the fields following the shared ObjControllerMessage envelope
    // (header1/header2/objectId/unused already consumed).
    static AddBuffMessage parse(soe::PacketBuffer& buf);
};

// Removes a buff from the player's buff window - the same generic Buff
// system's sibling message, sent when a buff (e.g. /burstrun's, confirmed
// live above) expires or is cancelled. SYNTHETIC TEST ONLY - burstrun's
// real cooldown made a live capture unreliable to time within this
// session's capture windows (two attempts: the first buff never appeared
// removed within the observation window, the second attempt was itself
// blocked by burstrun's own cooldown). Implemented directly from
// Buffs.h's constructor (insertInt(buffcrc), the SAME field type already
// confirmed live via AddBuffMessage's sibling field) - a much lower-risk
// case than CommandQueueAdd/Flourish, not a protocol gap, just unlucky
// timing. Revisit with a real fixture if convenient.
struct RemoveBuffMessage {
    uint32_t buffCrc = 0;

    // Parses the fields following the shared ObjControllerMessage envelope
    // (header1/header2/objectId/unused already consumed).
    static RemoveBuffMessage parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
