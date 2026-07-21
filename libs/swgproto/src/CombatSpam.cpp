#include "swgproto/CombatSpam.h"

namespace swgproto {

CombatSpam CombatSpam::parse(soe::PacketBuffer& buf) {
    CombatSpam result;
    result.attackerId = buf.readUint64();
    result.defenderId = buf.readUint64();
    result.itemId = buf.readUint64();
    result.damage = buf.readUint32();
    result.file = buf.readAscii();
    result.padding = buf.readUint32();
    result.stringName = buf.readAscii();
    result.color = buf.readByte();
    result.message = buf.readUnicode();
    return result;
}

} // namespace swgproto
