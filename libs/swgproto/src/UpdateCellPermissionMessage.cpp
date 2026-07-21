#include "swgproto/UpdateCellPermissionMessage.h"

namespace swgproto {

UpdateCellPermissionMessage UpdateCellPermissionMessage::parse(soe::PacketBuffer& buf) {
    UpdateCellPermissionMessage result;

    result.allowEntry = buf.readByte() != 0;
    result.cellObjectId = buf.readUint64();

    return result;
}

} // namespace swgproto
