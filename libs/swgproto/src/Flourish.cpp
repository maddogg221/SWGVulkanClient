#include "swgproto/Flourish.h"

namespace swgproto {

Flourish Flourish::parse(soe::PacketBuffer& buf) {
    Flourish result;
    result.flourishId = static_cast<int32_t>(buf.readUint32());
    return result;
}

} // namespace swgproto
