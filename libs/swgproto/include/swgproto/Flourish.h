#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"

namespace swgproto {

// header2 for this sub-message - confirmed via Core3's Flourish.h:
// ObjectControllerMessage(creo->getObjectID(), 0x1B, 0x166). Note header1
// is 0x1B here, NOT the 0x0B used by most other sub-types in this project.
constexpr uint32_t kFlourishControllerType = 0x166;

// A dance/music flourish during an active Entertainer performance
// (EntertainingSessionImplementation - only fires while genuinely dancing
// or playing music, or at outro with flourishId=-1). SYNTHETIC TEST ONLY -
// getting a real capture requires an Entertainer skill grant plus a valid
// dance/music performance name (checked live 2026-07-18: an empty
// /startdance on the admin test character came back with zero available
// dances - no Entertainer skill on hand). Implemented directly from
// Flourish.h's constructor (a single unambiguous insertInt(flourishid)) -
// same "implement from source, flag as unverified" treatment as
// CommandQueueAdd/CombatAction/CombatSpam. Revisit with a real fixture if
// an Entertainer character/skill grant is ever set up.
struct Flourish {
    int32_t flourishId = 0;

    // Parses the fields following the shared ObjControllerMessage envelope
    // (header1/header2/objectId/unused already consumed).
    static Flourish parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
