#include "swgproto/PlayerObjectBaseline8.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<PlayerObjectBaseline8> PlayerObjectBaseline8::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<PlayerObjectBaseline8>(kPlayerObjectBaseline8Schema, buf);
}

} // namespace swgproto
