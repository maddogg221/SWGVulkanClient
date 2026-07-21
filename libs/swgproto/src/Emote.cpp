#include "swgproto/Emote.h"

namespace swgproto {

Emote Emote::parse(soe::PacketBuffer& buf) {
    Emote result;
    result.senderId = buf.readUint64();
    result.emoteTargetId = buf.readUint64();
    result.emoteId = buf.readUint32();
    result.animTextFlags = buf.readByte();
    return result;
}

} // namespace swgproto
