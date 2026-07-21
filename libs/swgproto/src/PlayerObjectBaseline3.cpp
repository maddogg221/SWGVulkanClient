#include "swgproto/PlayerObjectBaseline3.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<PlayerObjectBaseline3> PlayerObjectBaseline3::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<PlayerObjectBaseline3>(kPlayerObjectBaseline3Schema, buf);
}

} // namespace swgproto
