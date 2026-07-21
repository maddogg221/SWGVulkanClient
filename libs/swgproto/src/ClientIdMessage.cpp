#include "swgproto/ClientIdMessage.h"

#include "soe/MessageHash.h"
#include "soe/PacketBuffer.h"

namespace swgproto {

std::vector<uint8_t> buildClientIdMessage(uint32_t accountId,
                                           const std::vector<uint8_t>& sessionToken,
                                           const std::string& version) {
    soe::PacketBuffer buf;
    buf.writeUint16(0x03); // opCount
    buf.writeUint32(soe::MessageHash::compute("ClientIdMsg"));

    buf.writeUint32(0xFE); // gameBits
    buf.writeUint32(static_cast<uint32_t>(sessionToken.size() + 4)); // dataLen: token + accountId
    buf.writeBytes(sessionToken);
    buf.writeUint32(accountId);
    buf.writeAscii(version);

    return std::vector<uint8_t>(buf.data(), buf.data() + buf.size());
}

} // namespace swgproto
