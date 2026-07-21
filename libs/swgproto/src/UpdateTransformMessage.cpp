#include "swgproto/UpdateTransformMessage.h"

namespace swgproto {

UpdateTransformMessage UpdateTransformMessage::parse(soe::PacketBuffer& buf) {
    UpdateTransformMessage result;

    result.objectId = buf.readUint64();
    result.x = static_cast<int16_t>(buf.readUint16()) / 4.0f;
    result.y = static_cast<int16_t>(buf.readUint16()) / 4.0f;
    result.z = static_cast<int16_t>(buf.readUint16()) / 4.0f;
    result.movementCounter = buf.readUint32();
    result.speed = static_cast<int8_t>(buf.readByte());
    result.direction = static_cast<int8_t>(buf.readByte());

    return result;
}

} // namespace swgproto
