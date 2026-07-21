#include "swgproto/SpatialChat.h"

namespace swgproto {

SpatialChat SpatialChat::parse(soe::PacketBuffer& buf) {
    SpatialChat result;
    result.senderId = buf.readUint64();
    result.chatTargetId = buf.readUint64();
    result.message = buf.readUnicode();
    result.volume = buf.readUint16();
    result.spatialChatType = buf.readUint16();
    result.moodType = buf.readUint16();
    result.chatFlags = buf.readByte();
    result.languageId = buf.readByte();
    result.unknownTrailingLong = buf.readUint64();
    result.unknownTrailingInt1 = static_cast<int32_t>(buf.readUint32());
    result.unknownTrailingInt2 = static_cast<int32_t>(buf.readUint32());
    return result;
}

} // namespace swgproto
