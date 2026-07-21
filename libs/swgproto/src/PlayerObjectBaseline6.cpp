#include "swgproto/PlayerObjectBaseline6.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<PlayerObjectBaseline6> PlayerObjectBaseline6::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<PlayerObjectBaseline6>(kPlayerObjectBaseline6Schema, buf);
}

} // namespace swgproto
