#include "swgproto/LoginClientToken.h"

#include <stdexcept>

namespace swgproto {

LoginClientToken LoginClientToken::parse(soe::PacketBuffer& buf) {
    LoginClientToken result;

    uint32_t tokenPlusFour = buf.readUint32();
    if (tokenPlusFour < 4) {
        throw std::runtime_error("LoginClientToken: invalid token length field");
    }
    uint32_t tokenLen = tokenPlusFour - 4;
    result.sessionToken = buf.readBytes(tokenLen);
    result.accountId = buf.readUint32();
    result.stationId = buf.readUint32();
    result.username = buf.readAscii();

    return result;
}

} // namespace swgproto
