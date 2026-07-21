#include "swgproto/ClientPermissionsMessage.h"

namespace swgproto {

ClientPermissionsMessage ClientPermissionsMessage::parse(soe::PacketBuffer& buf) {
    ClientPermissionsMessage result;
    result.canConnect = buf.readByte() != 0;
    result.canCreateCharacter = buf.readByte() != 0;
    result.ignoresCharCreationMax = buf.readByte() != 0;
    result.unknown = buf.readByte() != 0;
    return result;
}

} // namespace swgproto
