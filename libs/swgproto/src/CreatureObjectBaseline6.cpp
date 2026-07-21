#include "swgproto/CreatureObjectBaseline6.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<CreatureObjectBaseline6> CreatureObjectBaseline6::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<CreatureObjectBaseline6>(kCreatureObjectBaseline6Schema, buf);
}

} // namespace swgproto
