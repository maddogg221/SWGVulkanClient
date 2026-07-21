#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"

namespace swgproto {

// Hardcoded literal in Core3's own SceneObjectDestroyMessage.h - confirmed
// to also equal hashCode("SceneDestroyObject"), the reference client's own
// dispatch table case name (ZonePacketHandler.cpp), matching the same
// naming convention already used for this message's sibling,
// SceneCreateObjectByCrc.
//
// This is the general "remove this object from the world" signal - traced
// its real server-side trigger (SceneObjectImplementation::
// destroyObjectFromWorld() -> broadcastDestroy() -> sendDestroyTo() per
// nearby observer) to dozens of real call sites across the whole server:
// item/structure deletion, corpse decay, GCW installation destruction
// (turrets/minefields/scanners), pet/lair despawns, auction sales, and
// more. Confirmed NOT sent for static world objects
// (SceneObjectImplementation::sendDestroyTo() early-returns if
// `staticObject`), which never truly despawn.
constexpr uint32_t kSceneDestroyObjectHash = 0x4D45D504;

struct SceneDestroyObject {
    uint64_t objectId = 0;
    bool hyperspacing = false;

    // Parses the fields following opCount+hash.
    static SceneDestroyObject parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
