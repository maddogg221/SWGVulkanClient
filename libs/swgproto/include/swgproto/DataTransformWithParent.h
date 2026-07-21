#pragma once

#include <cstdint>
#include <vector>

#include "soe/PacketBuffer.h"

namespace swgproto {

constexpr uint32_t kDataTransformWithParentControllerType = 0xF1;

// Same "idle-state synchronize confirmation" purpose as DataTransform (see
// DataTransform.h for the full explanation, including the corrected-live
// "not actually self-only" finding and the same timeStamp/moveCount
// field-count caveat), for the parented/cell-local case. Analogous to
// UpdateTransformWithParentMessage for the broadcast/real-movement case.
struct DataTransformWithParent {
    uint32_t counter = 0;
    uint64_t parentId = 0; // comes right after counter, before direction
    float directionX = 0.0f;
    float directionY = 0.0f;
    float directionZ = 0.0f;
    float directionW = 0.0f;
    // Wire order X, [height], [other horizontal] - see DataTransform.h's
    // own comment for the full story on these field names.
    float x = 0.0f;
    float y = 0.0f; // vertical height (cell-local)
    float z = 0.0f; // the other horizontal coordinate (cell-local)
    float speed = 0.0f;

    // Parses the fields following the shared ObjControllerMessage envelope.
    // Wire layout: uint32 counter + uint64 parentId + float
    // dirX,dirY,dirZ,dirW + float x,(height),(other horizontal) + float
    // speed.
    static DataTransformWithParent parse(soe::PacketBuffer& buf);
};

// Builds the CLIENT->SERVER outbound shape (Phase 17 - cell-relative
// movement), the parented counterpart to buildDataTransform() (see that
// function's own comment for the shared envelope/field-count reasoning).
// Wire layout confirmed directly from Core3's own
// Transform::parseDataTransformWithParent() (server-side code that reads
// what a real client sends): uint32 timeStamp + uint32 moveCount + uint64
// parentID + float dirX,dirY,dirZ,dirW (quaternion) + float x,(height),
// (other horizontal) + float speed - identical to buildDataTransform()'s
// own shape with one extra uint64 parentId inserted right after moveCount.
// Sending this instead of the plain world-space buildDataTransform() while
// indoors is REQUIRED, not cosmetic: a real, live-caught bug this fixes -
// Core3's own server log showed real "Player Speed Abnormality" anti-cheat
// errors (computed speed 82+ units/sec against a 5.376 max) because the
// server, having already validated self's containment in a real cell via
// UpdateContainmentMessage, rejected a subsequent world-space position
// report as physically impossible - not just wrong-looking traffic, but
// something the server actively flags and reacts to.
std::vector<uint8_t> buildDataTransformWithParent(uint64_t objectId, uint32_t timeStamp,
                                                    uint32_t moveCount, uint64_t parentId,
                                                    float dirX, float dirY, float dirZ,
                                                    float dirW, float x, float y, float z,
                                                    float speed);

} // namespace swgproto
