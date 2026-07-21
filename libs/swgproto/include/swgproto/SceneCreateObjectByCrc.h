#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"

namespace swgproto {

// Hardcoded literal in Core3's own SceneObjectCreateMessage.h - confirmed
// in Phase 2 step 1 to also equal hashCode("SceneCreateObjectByCrc").
constexpr uint32_t kSceneCreateObjectByCrcHash = 0xFE89DDEA;

struct SceneCreateObjectByCrc {
    uint64_t objectId = 0;
    // Orientation quaternion. DISCOVERY.txt originally recorded these 16
    // bytes as "unknown/unparsed" (the reference client's own parser skips
    // them via shiftOffset(16), never decoding them) - reading Core3's
    // SERVER-side SceneObjectCreateMessage.h constructor (not just the
    // client's truncated parser) identified them.
    float directionX = 0.0f;
    float directionY = 0.0f;
    float directionZ = 0.0f;
    float directionW = 0.0f;
    // Wire order X, [height], [other horizontal] - same convention as
    // UpdateTransformMessage (see that header's own comment for the full
    // story). FIXED 2026-07-19: this struct previously mirrored Core3's raw
    // Vector3 accessor names (x, z, y wire order into fields named x, z,
    // y), the same bug class already found in CmdStartScene and the four
    // Transform message types. This one is the most consequential of all
    // six instances - it's what seeds SELF's own spawn position via
    // ObjectStore::seedPosition() - so a wrong reading here put the
    // character's own starting height ~thousands of meters off from
    // frame one, before any movement message ever ran.
    float x = 0.0f;
    float y = 0.0f; // vertical height
    float z = 0.0f; // the other horizontal (north-south) coordinate
    uint32_t objectCrc = 0; // the object's template CRC - identifies which class to instantiate
    bool hyperspacing = false;

    // Parses the fields following opCount+hash. Wire layout: uint64
    // objectId + 4 floats (direction quaternion X,Y,Z,W) + float x + float
    // (height) + float (other horizontal) + uint32 objectCrc + byte
    // hyperspacing flag.
    static SceneCreateObjectByCrc parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
