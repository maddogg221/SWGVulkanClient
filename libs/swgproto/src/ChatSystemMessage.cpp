#include "swgproto/ChatSystemMessage.h"

namespace swgproto {

ChatSystemMessage ChatSystemMessage::parse(soe::PacketBuffer& buf) {
    ChatSystemMessage result;

    result.displayType = buf.readByte();
    result.message = buf.readUnicode();
    result.paramsSize = buf.readUint32();

    return result;
}

} // namespace swgproto
