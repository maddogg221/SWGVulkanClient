#include "swgproto/CreatureObjectBaseline3.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<CreatureObjectBaseline3> CreatureObjectBaseline3::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<CreatureObjectBaseline3>(kCreatureObjectBaseline3Schema, buf);
}

} // namespace swgproto
