#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"

namespace swgproto {

// Confirmed both by reading Core3's UpdateTransformWithParentMessage.h
// (hardcoded literal) and independently by computing
// soe::MessageHash::compute("UpdateTransformWithParentMessage").
constexpr uint32_t kUpdateTransformWithParentMessageHash = 0xC867AB5A;

// Broadcast for an object's position update when it's inside a parent/cell
// (a CellObject or similar container) - position is LOCAL to parentId, not
// world coordinates (see UpdateTransformMessage for the unparented case).
// Pure server-to-client, same as UpdateTransformMessage.
struct UpdateTransformWithParentMessage {
    uint64_t parentId = 0; // the cell/container object ID - comes BEFORE objectId on the wire
    uint64_t objectId = 0;
    // Wire order is X, [height], [other horizontal], same convention as
    // UpdateTransformMessage - see that header's own comment for the full
    // story (this project's field names now match true meaning, not
    // Core3's raw Vector3 accessor names the wire order superficially
    // resembles). Raw wire values are signed int16 scaled by 8 (1/8-meter
    // granularity - finer than the unparented message, since cell-local
    // coordinates are always small), converted to float here.
    float x = 0.0f;
    float y = 0.0f; // vertical height (cell-local)
    float z = 0.0f; // the other horizontal coordinate (cell-local)
    uint32_t movementCounter = 0;
    int8_t speed = 0;
    int8_t direction = 0;

    // Parses the fields following opCount+hash. Wire layout: uint64 parentId
    // + uint64 objectId + int16 x + int16 (height) + int16 (other
    // horizontal) (each /8) + uint32 movementCounter + int8 speed + int8
    // direction.
    static UpdateTransformWithParentMessage parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
