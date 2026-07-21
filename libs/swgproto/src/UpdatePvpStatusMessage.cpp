#include "swgproto/UpdatePvpStatusMessage.h"

namespace swgproto {

UpdatePvpStatusMessage UpdatePvpStatusMessage::parse(soe::PacketBuffer& buf) {
    UpdatePvpStatusMessage result;

    result.pvpStatusBitmask = buf.readUint32();
    result.faction = buf.readUint32();
    result.objectId = buf.readUint64();

    return result;
}

} // namespace swgproto
