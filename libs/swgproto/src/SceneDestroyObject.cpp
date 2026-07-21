#include "swgproto/SceneDestroyObject.h"

namespace swgproto {

SceneDestroyObject SceneDestroyObject::parse(soe::PacketBuffer& buf) {
    SceneDestroyObject result;
    result.objectId = buf.readUint64();
    result.hyperspacing = buf.readByte() != 0;
    return result;
}

} // namespace swgproto
