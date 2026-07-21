#include "swgproto/GroupObjectBaseline6.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<GroupObjectBaseline6> GroupObjectBaseline6::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<GroupObjectBaseline6>(kGroupObjectBaseline6Schema, buf);
}

} // namespace swgproto
