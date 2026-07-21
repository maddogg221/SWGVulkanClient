#pragma once

#include <cstdint>

#include "soe/MessageHash.h"
#include "soe/PacketBuffer.h"

namespace swgproto {

// Computed via hashCode("SceneEndBaselines") - confirmed in Phase 2 step 1
// against the reference client's dispatch table.
constexpr uint32_t kSceneEndBaselinesHash = soe::MessageHash::compute("SceneEndBaselines");

struct SceneEndBaselines {
    uint64_t objectId = 0;

    // Parses the fields following opCount+hash. Wire layout: uint64
    // objectId - this is the ENTIRE payload. Unlike every other message in
    // this library, no source file for this one was found anywhere in
    // Core3's checked-in source (see DISCOVERY.txt's "PHASE 2 STEP 3"
    // section) - it's likely emitted from per-object-type generated code,
    // not a hand-written class. This layout was instead confirmed directly
    // from real captured Finalizer bytes: every sample observed was
    // exactly 8 bytes, matching known object IDs seen elsewhere in the
    // same session (and the object's own SceneCreateObjectByCrc).
    static SceneEndBaselines parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
