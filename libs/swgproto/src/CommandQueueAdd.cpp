#include "swgproto/CommandQueueAdd.h"

namespace swgproto {

CommandQueueAdd CommandQueueAdd::parse(soe::PacketBuffer& buf) {
    CommandQueueAdd result;
    result.actionCount = buf.readUint32();
    result.actionCrc = buf.readUint32();
    return result;
}

} // namespace swgproto
