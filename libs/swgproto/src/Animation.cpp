#include "swgproto/Animation.h"

namespace swgproto {

Animation Animation::parse(soe::PacketBuffer& buf) {
    Animation result;
    result.animationName = buf.readAscii();
    return result;
}

} // namespace swgproto
