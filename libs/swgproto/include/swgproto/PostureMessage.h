#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"

namespace swgproto {

constexpr uint32_t kPostureMessageControllerType = 0x131;

// A creature's posture (stance) changed - e.g. standing/crouched/prone/
// sitting. Confirmed via Core3's PostureMessage.h:
// ObjectControllerMessage(objid, 0x1B, 0x131), body: insertByte(posture) +
// insertByte(0x01). The second byte's purpose isn't documented in source -
// every real instance in source is a literal 0x01; kept as a raw field
// rather than assumed constant until live traffic proves it's always 1.
struct PostureMessage {
    // CreatureObject posture enum, per Core3's Transform::isPostureValid:
    // 0=upright, 1=crouched, 2=prone, 3=sneaking, 8=sitting,
    // 9=skill-animating (other posture values exist too, just aren't
    // movement-eligible per that check).
    uint8_t posture = 0;
    uint8_t unknownFlag = 0;

    // Parses the fields following the shared ObjControllerMessage envelope.
    // Wire layout: byte posture + byte unknownFlag.
    static PostureMessage parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
