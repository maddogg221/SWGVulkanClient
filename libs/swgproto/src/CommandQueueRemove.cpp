#include "swgproto/CommandQueueRemove.h"

namespace swgproto {

CommandQueueRemove CommandQueueRemove::parse(soe::PacketBuffer& buf) {
    CommandQueueRemove result;

    result.actionCount = buf.readUint32();
    result.timer = buf.readFloat();
    result.tab1 = buf.readUint32();
    result.tab2 = buf.readUint32();

    return result;
}

} // namespace swgproto
