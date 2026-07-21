#include "swgproto/AccountVersionMessage.h"

#include "soe/MessageHash.h"
#include "soe/PacketBuffer.h"

namespace swgproto {

std::vector<uint8_t> buildAccountVersionMessage(const std::string& username,
                                                 const std::string& password,
                                                 const std::string& version) {
    soe::PacketBuffer buf;
    buf.writeUint16(0x04); // opCount
    buf.writeUint32(soe::MessageHash::compute("LoginClientId"));
    buf.writeAscii(username);
    buf.writeAscii(password);
    buf.writeAscii(version);

    return std::vector<uint8_t>(buf.data(), buf.data() + buf.size());
}

} // namespace swgproto
