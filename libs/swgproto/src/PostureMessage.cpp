#include "swgproto/PostureMessage.h"

namespace swgproto {

PostureMessage PostureMessage::parse(soe::PacketBuffer& buf) {
    PostureMessage result;
    result.posture = buf.readByte();
    result.unknownFlag = buf.readByte();
    return result;
}

} // namespace swgproto
