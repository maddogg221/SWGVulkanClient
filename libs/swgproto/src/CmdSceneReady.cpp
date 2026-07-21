#include "swgproto/CmdSceneReady.h"

#include "soe/PacketBuffer.h"

namespace swgproto {

std::vector<uint8_t> buildCmdSceneReady() {
    soe::PacketBuffer buf;
    buf.writeUint16(0x01); // opCount
    buf.writeUint32(kCmdSceneReadyHash);

    return std::vector<uint8_t>(buf.data(), buf.data() + buf.size());
}

} // namespace swgproto
