#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"

namespace swgproto {

// Confirmed both by reading Core3's UpdateTransformMessage.h (hardcoded literal)
// and independently by computing soe::MessageHash::compute("UpdateTransformMessage").
constexpr uint32_t kUpdateTransformMessageHash = 0x1B24F808;

// Broadcast for an object's world-position update (no parent/cell - see
// UpdateTransformWithParentMessage for the parented/cell-local case). Pure
// server-to-client; the client never sends this shape back (movement intent
// is DataTransform/DataTransformWithParent, a different message entirely).
struct UpdateTransformMessage {
    uint64_t objectId = 0;
    // Wire order is X, [height], [other horizontal] - Core3's own
    // CmdStartScene.h sends a raw engine::util::u3d::Vector3's
    // getX()/getZ()/getY() in that order (its own inline comments literally
    // say "//X" "//Z" "//Y"), and UpdateTransformMessage shares the exact
    // same convention. Those Core3 accessor NAMES do NOT match this
    // codebase's own established meaning (y=height, used consistently
    // elsewhere and confirmed correct by every live-rendered object so
    // far) - Vector3::getZ() (the wire's 2nd float) is the one that's
    // actually vertical height, and Vector3::getY() (the wire's 3rd float)
    // is the other horizontal coordinate. FIXED 2026-07-19: this struct's
    // fields previously mirrored Core3's raw accessor names (z=2nd wire
    // float, y=3rd), which happened to render self-consistently (every
    // consumer - the ground grid, wireframe boxes, the camera - re-derives
    // its own reference from the same, uniformly-swapped obj.y/obj.z, so
    // nothing looked wrong relative to itself) until real terrain
    // (computed independently, from real per-planet generation data, and
    // live-verified against real ground height) exposed the mismatch: self
    // rendered ~2500m away from its own correct ground height. See
    // CmdStartScene.h for the original discovery of this bug class. Raw
    // wire values are signed int16 scaled by 4 (1/4-meter granularity);
    // converted to float here so position values are uniformly float
    // across this codebase.
    float x = 0.0f;
    float y = 0.0f; // vertical height
    float z = 0.0f; // the other horizontal (north-south) coordinate
    uint32_t movementCounter = 0; // monotonic per-object sequence number
    int8_t speed = 0;             // 0 for non-CreatureObject objects
    int8_t direction = 0;         // yaw-only, scaled 0-100 (not degrees) - lossy heading

    // Parses the fields following opCount+hash. Wire layout: uint64 objectId
    // + int16 x + int16 (height) + int16 (other horizontal) (each /4) +
    // uint32 movementCounter + int8 speed + int8 direction.
    static UpdateTransformMessage parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
