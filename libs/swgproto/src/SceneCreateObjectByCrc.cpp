#include "swgproto/SceneCreateObjectByCrc.h"

namespace swgproto {

SceneCreateObjectByCrc SceneCreateObjectByCrc::parse(soe::PacketBuffer& buf) {
    SceneCreateObjectByCrc result;

    result.objectId = buf.readUint64();
    result.directionX = buf.readFloat();
    result.directionY = buf.readFloat();
    result.directionZ = buf.readFloat();
    result.directionW = buf.readFloat();
    result.x = buf.readFloat();
    result.y = buf.readFloat();
    result.z = buf.readFloat();
    result.objectCrc = buf.readUint32();
    result.hyperspacing = buf.readByte() != 0;

    return result;
}

} // namespace swgproto
