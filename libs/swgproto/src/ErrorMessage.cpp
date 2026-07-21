#include "swgproto/ErrorMessage.h"

namespace swgproto {

ErrorMessage ErrorMessage::parse(soe::PacketBuffer& buf) {
    ErrorMessage result;
    result.errorType = buf.readAscii();
    result.errorMsg = buf.readAscii();
    result.fatal = buf.readByte();
    return result;
}

} // namespace swgproto
