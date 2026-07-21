#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"

namespace swgproto {

constexpr uint32_t kCommandQueueRemoveControllerType = 0x117;

// Signals that a queued command/action has completed and been removed from
// the creature's command queue - fires once per attack/ability execution in
// real combat traffic (see DISCOVERY.txt's "PHASE 4 STEP 3/5 COMPLETE" -
// this is what actually pairs with ShowFlyText hit-location feedback,
// firing right before or after each hit string). Confirmed via Core3's
// CommandQueueRemove.h: ObjectControllerMessage(creo->getObjectID(), 0x1B,
// 0x117), body: insertInt(actioncnt) + insertFloat(timer) +
// insertInt(tab1) + insertInt(tab2). NOTE: Core3's own
// CommandQueueRemoveCallback (the CLIENT->SERVER direction of this same
// header2) parses a COMPLETELY DIFFERENT 3-field shape (size/actionCount/
// actionCRC) - the same asymmetry already seen with DataTransform/
// DataTransformWithParent (Phase 4 step 2). This struct follows the
// CONSTRUCTOR's fields since that's what a passive client actually
// receives.
struct CommandQueueRemove {
    // Per Core3's own parameter name. Real captured traffic (both Finalizer
    // and an unmodified public Core3 server - see DISCOVERY.txt's "PHASE 4
    // STEP 6 COMPLETE") shows this field's low bits increment by exactly 32
    // between consecutive real instances on the same connection - consistent
    // with a genuine monotonic action-queue counter - while bit 30
    // (0x40000000) toggles independently of that increment. What that bit
    // means is NOT confirmed (no source evidence either way), so this is
    // kept as one opaque raw uint32 rather than split into a guessed
    // counter+flag pair.
    uint32_t actionCount = 0;
    float timer = 0.0f;    // seconds; 0 in several real samples, ~1.7-2.6 in others
    uint32_t tab1 = 0;     // per Core3's own (unexplained) parameter name
    uint32_t tab2 = 0;     // per Core3's own (unexplained) parameter name; 0 in every real sample seen

    // Parses the fields following the shared ObjControllerMessage envelope
    // (header1/header2/objectId/unused already consumed). Wire layout:
    // uint32 actionCount + float timer + uint32 tab1 + uint32 tab2 (16
    // bytes total).
    static CommandQueueRemove parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
