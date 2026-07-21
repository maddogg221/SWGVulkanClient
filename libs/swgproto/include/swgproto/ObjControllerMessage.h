#pragma once

#include <cstdint>

#include "soe/MessageHash.h"
#include "soe/PacketBuffer.h"

namespace swgproto {

// Computed via hashCode("ObjControllerMessage") - confirmed in Phase 2
// step 1 against the reference client's dispatch table.
constexpr uint32_t kObjControllerMessageHash = soe::MessageHash::compute("ObjControllerMessage");

// A generic envelope, same as BaselineEnvelope - the reference client
// itself doesn't decode anything beyond these 4 fields (its own handler is
// a no-op past parsing them), so this is genuinely everything known about
// this message's layout, not an intentionally-partial parse.
struct ObjControllerMessage {
    uint32_t header1 = 0;
    uint32_t header2 = 0;
    uint64_t objectId = 0;
    uint32_t unused = 0;

    // Parses the fields following opCount+hash. Wire layout: uint32
    // header1 + uint32 header2 + uint64 objectId + uint32 [unused].
    static ObjControllerMessage parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
