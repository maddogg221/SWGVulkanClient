#include "swgproto/SceneEndBaselines.h"

namespace swgproto {

SceneEndBaselines SceneEndBaselines::parse(soe::PacketBuffer& buf) {
    SceneEndBaselines result;
    result.objectId = buf.readUint64();
    return result;
}

} // namespace swgproto
