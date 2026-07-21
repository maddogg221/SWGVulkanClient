#include "swgproto/UpdateContainmentMessage.h"

namespace swgproto {

UpdateContainmentMessage UpdateContainmentMessage::parse(soe::PacketBuffer& buf) {
    UpdateContainmentMessage result;

    result.objectId = buf.readUint64();
    result.containerId = buf.readUint64();
    result.type = static_cast<int32_t>(buf.readUint32());

    return result;
}

} // namespace swgproto
