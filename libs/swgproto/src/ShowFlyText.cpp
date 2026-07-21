#include "swgproto/ShowFlyText.h"

namespace swgproto {

ShowFlyText ShowFlyText::parse(soe::PacketBuffer& buf) {
    ShowFlyText result;

    result.targetObjectId = buf.readUint64();
    result.file = buf.readAscii();
    result.spacer = buf.readUint32();
    result.entry = buf.readAscii();
    result.scale = buf.readFloat();
    result.red = buf.readByte();
    result.green = buf.readByte();
    result.blue = buf.readByte();
    result.flags = buf.readByte();

    return result;
}

} // namespace swgproto
