#include "swgproto/PlayerObjectBaseline9.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<PlayerObjectBaseline9> PlayerObjectBaseline9::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<PlayerObjectBaseline9>(kPlayerObjectBaseline9Schema, buf);
}

} // namespace swgproto
