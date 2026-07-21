#include "swgproto/Buffs.h"

namespace swgproto {

AddBuffMessage AddBuffMessage::parse(soe::PacketBuffer& buf) {
    AddBuffMessage result;
    result.buffCrc = buf.readUint32();
    result.duration = buf.readFloat();
    return result;
}

RemoveBuffMessage RemoveBuffMessage::parse(soe::PacketBuffer& buf) {
    RemoveBuffMessage result;
    result.buffCrc = buf.readUint32();
    return result;
}

} // namespace swgproto
