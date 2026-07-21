#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"

namespace swgproto {

// header2 (the ObjControllerMessage "type" discriminator) for this
// sub-message - see ObjControllerMessage.h for the shared envelope this
// rides inside. Confirmed via Core3's Animation.h:
// ObjectControllerMessage(objid, 0x1B, 0xF2, false).
constexpr uint32_t kAnimationControllerType = 0xF2;

// A one-shot animation cue (e.g. crafting/social animations) - the most
// frequent ObjControllerMessage sub-type observed live (Phase 4 step 1).
// Server->client only; purely presentational, no gameplay state.
struct Animation {
    std::string animationName;

    // Parses the fields following the shared ObjControllerMessage envelope
    // (header1/header2/objectId/unused already consumed). Wire layout:
    // ASCII animationName.
    static Animation parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
