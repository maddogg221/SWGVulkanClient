#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"

namespace swgproto {

// header2 for this sub-message - confirmed via Core3's CommandQueueAdd.h:
// ObjectControllerMessage(creo->getObjectID(), 0x0B, 0x167).
constexpr uint32_t kCommandQueueAddControllerType = 0x167;

// Signals a server-initiated action being queued. NOT sent for normal
// client-issued commands - traced every real caller of
// CreatureObject::sendCommand() (the only path that emits this) in Core3's
// source and confirmed they're all server-triggered (stimpack/medpack use,
// grenades, traps, heavy-weapon-mount firing), reached exclusively via
// right-click radial menu selection (handleObjectMenuSelect), which needs
// this project's still-undecoded SUI/radial-menu protocol
// (ObjectMenuResponse, Phase 4 Step 12) to trigger for real. Implemented
// directly from source (CommandQueueAdd.h's constructor gives an
// unambiguous 8-byte shape) with a synthetic-only test - same treatment as
// CombatAction/CombatSpam (Step 13), flagged rather than silently treated
// as real-traffic-confirmed. Revisit with a real capture once the
// radial-menu protocol is decoded.
struct CommandQueueAdd {
    uint32_t actionCount = 0;
    uint32_t actionCrc = 0;

    // Parses the fields following the shared ObjControllerMessage envelope
    // (header1/header2/objectId/unused already consumed).
    static CommandQueueAdd parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
