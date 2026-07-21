#include "swgproto/ObjControllerMessage.h"

namespace swgproto {

ObjControllerMessage ObjControllerMessage::parse(soe::PacketBuffer& buf) {
    ObjControllerMessage result;

    result.header1 = buf.readUint32();
    result.header2 = buf.readUint32();
    result.objectId = buf.readUint64();
    result.unused = buf.readUint32();

    return result;
}

} // namespace swgproto
