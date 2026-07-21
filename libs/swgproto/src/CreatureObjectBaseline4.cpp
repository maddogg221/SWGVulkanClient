#include "swgproto/CreatureObjectBaseline4.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<CreatureObjectBaseline4> CreatureObjectBaseline4::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<CreatureObjectBaseline4>(kCreatureObjectBaseline4Schema, buf);
}

} // namespace swgproto
