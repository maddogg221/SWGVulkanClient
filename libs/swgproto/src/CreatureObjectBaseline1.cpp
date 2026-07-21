#include "swgproto/CreatureObjectBaseline1.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<CreatureObjectBaseline1> CreatureObjectBaseline1::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<CreatureObjectBaseline1>(kCreatureObjectBaseline1Schema, buf);
}

} // namespace swgproto
